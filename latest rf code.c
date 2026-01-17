#include "HT68R004.h"

#define ENTER_SLEEP()   _halt()

/* ================= FORWARD DECLARATIONS ================= */
void handle_button(unsigned char b);

/* ================= GLOBALS ================= */

const unsigned char remote_id __attribute__ ((at(0x1FF0)));

bit led_active;

/* =========================================================
   GPIO INIT
   ========================================================= */
void io_init(void)
{
    _pac = 0x00;
    _pbc = 0x00;
    _pcc = 0x00;

    _pas0 = 0x00;
    _pbs0 = 0x00;
    _pcs0 = 0x00;

    /* -------- BUTTON MATRIX -------- */
    _pac &= ~((1<<3) | (1<<1));   // PA3, PA1 output
    _pcc &= ~((1<<2) | (1<<3));   // PC2, PC3 output

    _pa3 = 1;
    _pa1 = 1;
    _pc2 = 1;
    _pc3 = 1;

    _pcc |= (1<<0) | (1<<1);      // PC0, PC1 input
    _pac |= (1<<4) | (1<<5);      // PA4, PA5 input

    _pcpu |= (1<<0) | (1<<1);
    _papu |= (1<<4) | (1<<5);

    /* -------- LED CHARLIEPLEX -------- */
    _pbc |= 0x3E;                 // PB1–PB5 input (Hi-Z)

    /* -------- RF -------- */
    _pbc &= ~(1<<0);              // PB0 output
    _pb0 = 0;
}

/* =========================================================
   CHARLIEPLEX LED
   ========================================================= */

void led_all_hiz(void)
{
    _pbc |= 0x3E;
    _pb1=_pb2=_pb3=_pb4=_pb5=1;
}

void led_drive(unsigned char hi, unsigned char lo)
{
    led_all_hiz();

    _pbc &= ~((1<<hi) | (1<<lo));

    if(hi==1) _pb1=1;
    if(hi==2) _pb2=1;
    if(hi==3) _pb3=1;
    if(hi==4) _pb4=1;
    if(hi==5) _pb5=1;

    if(lo==1) _pb1=0;
    if(lo==2) _pb2=0;
    if(lo==3) _pb3=0;
    if(lo==4) _pb4=0;
    if(lo==5) _pb5=0;
}

void led_on(unsigned char n)
{
    led_active = 1;

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
    led_active = 0;
    led_all_hiz();
}

/* =========================================================
   BUTTON SCAN
   ========================================================= */
unsigned char scan_button(void)
{
    _pa3=_pa1=_pc2=_pc3=1;

    _pc3=0;
    if(!_pc0) return 1;
    if(!_pc1) return 2;
    if(!_pa4) return 13;
    if(!_pa5) return 14;
    _pc3=1;

    _pa3=0;
    if(!_pc1) return 3;
    if(!_pc0) return 4;
    _pa3=1;

    _pa1=0;
    if(!_pc0) return 5;
    if(!_pc1) return 6;
    if(!_pa4) return 7;
    if(!_pa5) return 8;
    _pa1=1;

    _pc2=0;
    if(!_pc0) return 9;
    if(!_pc1) return 10;
    if(!_pa4) return 11;
    if(!_pa5) return 12;
    _pc2=1;

    return 0;
}

/* =========================================================
   RF (ASK / OOK)
   ========================================================= */

#define RF_PIN _pb0
#define T_PREAMBLE 8
#define PREAMBLE_TOGGLES 60
#define T_SHORT 16
#define T_LONG 48
#define RF_REPEAT 10

unsigned char rf_busy, rf_repeat, rf_phase, rf_half;
unsigned char rf_preamble_cnt;
unsigned int rf_tick;
unsigned char rf_frame[4];

void RF_Request(unsigned char cmd)
{
    if(rf_busy) return;

    rf_busy = 1;
    rf_repeat = 0xFE;
    rf_phase = 0;
    rf_half = 0;
    rf_tick = 0;
    rf_preamble_cnt = PREAMBLE_TOGGLES;

    RF_PIN = 0;

    rf_frame[0]=0xAA;
    rf_frame[1]=0x0E;
    rf_frame[2]=remote_id;
    rf_frame[3]=cmd;
}

void RF_Service(void)
{
    unsigned char rf_bit;
    unsigned int period;

    if(!rf_busy) return;
    rf_tick++;

    if(rf_preamble_cnt)
    {
        if(rf_tick<T_PREAMBLE) return;
        rf_tick=0;
        rf_half^=1;
        RF_PIN=rf_half;
        rf_preamble_cnt--;
        return;
    }

    rf_bit=(rf_frame[rf_phase>>3]>>(7-(rf_phase&7)))&1;
    period=rf_bit?(rf_half?T_LONG:T_SHORT):(rf_half?T_SHORT:T_LONG);

    if(rf_tick<period) return;
    rf_tick=0;
    rf_half^=1;
    RF_PIN=rf_half;

    if(rf_half)
    {
        rf_phase++;
        if(rf_phase>=32)
        {
            rf_phase=0;
            rf_repeat++;
            if(rf_repeat>=RF_REPEAT)
            {
                rf_busy=0;
                RF_PIN=0;
            }
            else rf_preamble_cnt=PREAMBLE_TOGGLES;
        }
    }
}

/* =========================================================
   APPLICATION LOGIC
   ========================================================= */

unsigned char power_on,eco_mode,turbo_mode,night_mode;
unsigned char under_light,breeze_mode;
unsigned char fan_speed,timer_hours,fan_direction;

unsigned int led_timer;
unsigned char last_cmd;

void handle_button(unsigned char b)
{
    switch(b)
    {
        case 1: power_on=!power_on; break;
        case 2: eco_mode=1; turbo_mode=0; fan_speed=5; break;
        case 3: turbo_mode=1; eco_mode=0; fan_speed=6; break;
        case 4: fan_speed=1; break;
        case 5: fan_speed=2; break;
        case 6: fan_speed=3; break;
        case 7: fan_speed=4; break;
        case 8: fan_speed=5; break;
        case 9: fan_direction=!fan_direction; break;
        case 10: night_mode=1; fan_speed=1; break;
        case 11: under_light=!under_light; break;
        case 12:
            if(timer_hours==0) timer_hours=1;
            else if(timer_hours==1) timer_hours=3;
            else if(timer_hours==3) timer_hours=5;
            else timer_hours=0;
            break;
        case 13: breeze_mode=!breeze_mode; break;
    }
}

/* =========================================================
   MAIN (WITH STARTUP LED INDICATION)
   ========================================================= */

void main(void)
{
    unsigned char curr;
    unsigned int startup_timer;
    unsigned char startup_led;

    _wdtc=0x57;
    io_init();

    rf_busy=rf_repeat=rf_phase=rf_half=0;
    rf_tick=0;
    RF_PIN=0;

    last_cmd=0;
    led_timer=0;

    _emi=1;

    /* ===== STARTUP LED SEQUENCE (OPTION A) ===== */
    startup_timer = 0;
    startup_led = 1;

    while(startup_timer < 20)
    {
        led_on(startup_led);
        startup_led++;
        if(startup_led > 15) startup_led = 1;
        startup_timer++;
    }

    leds_off();
    /* ===== END STARTUP ===== */

    while(1)
    {
        curr=scan_button();

        if(curr && curr!=last_cmd)
        {
            last_cmd=curr;
            led_on(curr);
            led_timer=0;
            handle_button(curr);
            RF_Request(curr);
        }

        if(!curr)
        {
            last_cmd=0;
            rf_busy=0;
            RF_PIN=0;
        }

        if(led_timer<2000) led_timer++;
        if(led_timer==2000) leds_off();

        RF_Service();
    }
}
