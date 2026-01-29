#include "BX68R004.h"

#define ENTER_SLEEP()   _halt()
#define LED_ON_TIME 70
#define STARTUP_CYCLES 5  // Number of times to cycle through all LEDs at startup

void handle_button(unsigned char b);
void led_startup_sequence(void);

/* ================= GLOBALS ================= */
const unsigned char remote_id __attribute__ ((at(0x1FF0)));

unsigned char led_mode;
unsigned char mux_index;
unsigned char mux_tick;

unsigned char timer_mode;
unsigned char timer_index;
unsigned char timer_clicks;
unsigned char timer_mux_tick;

unsigned int led_timer;
unsigned char last_cmd;

bit pair_hold_active;
bit pair_released;

/* ================= GPIO INIT ================= */
void io_init(void)
{
    _pac=_pbc=_pcc=0x00;
    _pas0=_pbs0=_pcs0=0x00;

    _pac &= ~((1<<3)|(1<<1));
    _pcc &= ~((1<<2)|(1<<3));

    _pa3=_pa1=_pc2=_pc3=1;

    _pcc |= (1<<0)|(1<<1);
    _pac |= (1<<4)|(1<<5);
    _pcpu |= (1<<0)|(1<<1);
    _papu |= (1<<4)|(1<<5);

    _pbc |= 0x3E;
    _pbc &= ~(1<<0);
    _pb0=0;
}

/* ================= LED ================= */
void led_all_hiz(void)
{
    _pbc |= 0x3E;
    _pb1=_pb2=_pb3=_pb4=_pb5=1;
}

void led_drive(unsigned char hi,unsigned char lo)
{
    led_all_hiz();
    _pbc &= ~((1<<hi)|(1<<lo));

    if(hi==1)_pb1=1;
    if(hi==2)_pb2=1;
    if(hi==3)_pb3=1;
    if(hi==4)_pb4=1;
    if(hi==5)_pb5=1;

    if(lo==1)_pb1=0;
    if(lo==2)_pb2=0;
    if(lo==3)_pb3=0;
    if(lo==4)_pb4=0;
    if(lo==5)_pb5=0;
}

void led_on(unsigned char n)
{
    switch(n)
    {
        case 1:  led_drive(1,2); break;
        case 2:  led_drive(2,1); break;
        case 3:  led_drive(1,3); break;
        case 4:  led_drive(3,1); break;
        case 5:  led_drive(2,3); break;
        case 6:  led_drive(3,2); break;
        case 7:  led_drive(1,4); break;
        case 8:  led_drive(4,1); break;
        case 9:  led_drive(2,4); break;
        case 10: led_drive(4,2); break;
        case 11: led_drive(3,5); break;
        case 12: led_drive(5,3); break;
        case 13: led_drive(4,5); break;
        case 14: led_drive(5,4); break;
        case 15: led_drive(1,5); break;
        default: led_all_hiz(); break;
    }
}

void leds_off(void)
{
    led_all_hiz();
    led_mode=0;
    timer_mode=0;
    pair_hold_active=0;
    pair_released=0;
}

/* ================= LED STARTUP SEQUENCE ================= */
void led_startup_sequence(void)
{
    unsigned char cycle;
    unsigned char led_num;
    
    cycle = 0;
    while(cycle < STARTUP_CYCLES)
    {
        led_num = 1;
        while(led_num <= 15)
        {
            led_on(led_num);
            _nop();
            _nop();
            led_num = led_num + 1;
        }
        cycle = cycle + 1;
    }
    
    led_all_hiz();
}

/* ================= BUTTON SCAN ================= */
unsigned char scan_button(void)
{
    _pa3=_pa1=_pc2=_pc3=1;

    _pc3=0;
    if(!_pc0)return 1;
    if(!_pc1)return 2;
    if(!_pa4)return 13;
    if(!_pa5)return 14;
    _pc3=1;

    _pa3=0;
    if(!_pc1)return 3;
    if(!_pc0)return 4;
    _pa3=1;

    _pa1=0;
    if(!_pc0)return 5;
    if(!_pc1)return 6;
    if(!_pa4)return 7;
    if(!_pa5)return 8;
    _pa1=1;

    _pc2=0;
    if(!_pc0)return 9;
    if(!_pc1)return 10;
    if(!_pa4)return 11;
    if(!_pa5)return 12;
    _pc2=1;

    return 0;
}

/* ================= SIMPLIFIED RF ================= */
#define RF_PIN _pb0
#define SYNC_PULSE 40
#define BIT_TIME 10
#define FRAME_GAP 40
#define RF_REPEAT 2

unsigned char rf_busy;

void rf_delay(unsigned char ticks)
{
    unsigned char i;
    i = 0;
    while(i < ticks)
    {
        _nop();
        _nop();
        _nop();
        _nop();
        i = i + 1;
    }
}

void RF_Send(unsigned char button)
{
    unsigned char mybit;
    unsigned char myrep;
    
    rf_busy = 1;
    
    myrep = 0;
    while(myrep < RF_REPEAT)
    {
        RF_PIN = 1;
        rf_delay(SYNC_PULSE);
        RF_PIN = 0;
        rf_delay(BIT_TIME);
        
        mybit = 0;
        while(mybit < 4)
        {
            if((button >> (3 - mybit)) & 1)
            {
                RF_PIN = 1;
                rf_delay(BIT_TIME);
                RF_PIN = 0;
            }
            else
            {
                RF_PIN = 0;
            }
            rf_delay(BIT_TIME);
            mybit = mybit + 1;
        }
        
        rf_delay(FRAME_GAP);
        myrep = myrep + 1;
    }
    
    RF_PIN = 0;
    rf_busy = 0;
}

/* ================= MAIN ================= */
void handle_button(unsigned char b){}

void main(void)
{
    unsigned char curr;

     _wdtc=0x57;
    io_init();

    rf_busy=0;
    RF_PIN=0;

    led_timer=0;
    last_cmd=0;
    led_mode=0;
    timer_mode=0;
    mux_index=0;
    mux_tick=0;
    timer_index=0;
    timer_clicks=0;
    timer_mux_tick=0;
    pair_hold_active=0;
    pair_released=0;

    _emi=1;

    /* *** LED STARTUP SEQUENCE - RUNS ONCE ON POWER-UP *** */
    led_startup_sequence();

	   while(1)
	{
	    curr=scan_button();
	
	    if(curr && curr!=last_cmd)
	    {
	        led_mode=0;
	        timer_mode=0;
	        pair_hold_active=0;
	        pair_released=0;
	        led_timer=0;
	        leds_off();
	
	        last_cmd=curr;
	        mux_index=0;
	        mux_tick=0;
	        timer_index=0;
	        timer_mux_tick=0;
	
	        if(curr!=12) timer_clicks=0;
	
	        if(curr==1)       led_on(12);
	        else if(curr==2)  led_on(9);
	        else if(curr==3)  led_on(15);
	        else if(curr==4)
	        {
	            led_all_hiz();
	            _pbc &= ~(1<<3);
	            _pb3=1;
	            _pbc &= ~((1<<1)|(1<<5));
	            _pb1=0;
	            _pb5=0;
	        }
	        else if(curr==5)  led_mode=5;
	        else if(curr==6)  led_mode=6;
	        else if(curr==7)  led_mode=7;
	        else if(curr==8)  led_mode=8;
	        else if(curr==9)  led_on(6);
	        else if(curr==10) led_on(13);
	        else if(curr==11) led_on(5);
	        else if(curr==12)
	        {
	            timer_clicks++;
	            if(timer_clicks>5) timer_clicks=1;
	            timer_mode=1;
	        }
	        else if(curr==13) led_on(10);
	        else if(curr==14)
	        {
	            pair_hold_active=1;
	            pair_released=0;
	
	            led_all_hiz();
	            _pbc &= ~(1<<3);
	            _pb3=0;
	            _pbc &= ~((1<<2)|(1<<5));
	            _pb2=1;
	            _pb5=1;
	        }
	    }
	
	    if(pair_hold_active && curr==14 && led_timer>=LED_ON_TIME)
	    {
	        led_all_hiz();
	        _pbc &= ~(1<<3);
	        _pb3=0;
	        _pbc &= ~(1<<2);
	        _pb2=1;
	    }
	
	    if(curr && !rf_busy)
	    {
	        RF_Send(curr);
	    }
	
	    if(!curr)
	    {
	        rf_busy=0;
	        RF_PIN=0;
	        last_cmd=0;
	        if(pair_hold_active) pair_released=1;
	    }
	
	    /* ===== FAST MULTIPLEX : SPEED BUTTONS ===== */
	
	    if(led_mode==5 && led_timer<LED_ON_TIME)
	    {
	        static const unsigned char l[]={7,11,14,2,1};
	        mux_tick++;
	        if(mux_tick>=1){ mux_tick=0; mux_index++; }
	        led_on(l[mux_index%5]);
	    }
	
	    if(led_mode==6 && led_timer<LED_ON_TIME)
	    {
	        static const unsigned char l[]={7,11,14,4,1};
	        mux_tick++;
	        if(mux_tick>=1){ mux_tick=0; mux_index++; }
	        led_on(l[mux_index%5]);
	    }
	
	    if(led_mode==7 && led_timer<LED_ON_TIME)
	    {
	        static const unsigned char l[]={8,14,11,4};
	        mux_tick++;
	        if(mux_tick>=1){ mux_tick=0; mux_index++; }
	        led_on(l[mux_index%4]);
	    }
	
	    if(led_mode==8 && led_timer<LED_ON_TIME)
	    {
	        static const unsigned char l[]={7,8,14,4,1};
	        mux_tick++;
	        if(mux_tick>=1){ mux_tick=0; mux_index++; }
	        led_on(l[mux_index%5]);
	    }
	
	    /* ===== FAST MULTIPLEX : TIMER BUTTON ===== */
	
	    if(timer_mode && led_timer<LED_ON_TIME)
	    {
	        static const unsigned char t1[]={3,11,4};
	        static const unsigned char t2[]={3,7,11,14,2,1};
	        static const unsigned char t3[]={3,8,14,11,4};
	        static const unsigned char t4[]={3,7,8,14,2,4,1};
	        static const unsigned char t5[]={3,7,8,11,14,2,4,1};
	
	        const unsigned char *p;
	        unsigned char len;
	
	        if(timer_clicks==1){p=t1;len=3;}
	        else if(timer_clicks==2){p=t2;len=6;}
	        else if(timer_clicks==3){p=t3;len=5;}
	        else if(timer_clicks==4){p=t4;len=7;}
	        else {p=t5;len=8;}
	
	        timer_mux_tick++;
	        if(timer_mux_tick>=1){ timer_mux_tick=0; timer_index++; }
	        led_on(p[timer_index%len]);
	    }
	
	    if(led_timer<LED_ON_TIME) led_timer++;
	
	    if(led_timer==LED_ON_TIME)
	    {
	        if(pair_hold_active && curr==14)
	        {
	            led_all_hiz();
	            _pbc &= ~(1<<3);
	            _pb3=0;
	            _pbc &= ~(1<<2);
	            _pb2=1;
	        }
	        else
	        {
	            leds_off();
	        }
	    }
	}
}
