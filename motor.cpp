//------------------------------------------------------------------------------
// Makelangelo - firmware for various robot kinematic models
// dan@marginallycelver.com 2013-12-26
// Please see http://www.github.com/MarginallyClever/makelangeloFirmware for more information.
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// INCLUDES
//------------------------------------------------------------------------------
#include "configure.h"
#include "motor.h"
#include "MServo.h"
#include "LCD.h"

//------------------------------------------------------------------------------
// MACROS
//------------------------------------------------------------------------------

#ifdef ESP8266
#define CLOCK_ADJUST(x) {  timer0_write(ESP.getCycleCount() + (long) (80000L*(x)) );  }  // microseconds
#else
#define CLOCK_ADJUST(x) {  OCR1A = (x);  }  // microseconds
#endif

//------------------------------------------------------------------------------
// DEFINES
//------------------------------------------------------------------------------

#define BLOCK_DELAY_FOR_1ST_MOVE 100
#define MIN_STEP_RATE 120

//------------------------------------------------------------------------------
// GLOBALS
//------------------------------------------------------------------------------

Motor motors[NUM_MOTORS + NUM_SERVOS];
#ifndef ESP8266
Servo servos[NUM_SERVOS];
#endif

Segment line_segments[MAX_SEGMENTS];
Segment *working_seg = NULL;
volatile int current_segment = 0;
volatile int last_segment = 0;
int first_segment_delay;

// used by timer1 to optimize interrupt inner loop
int steps_total;
int steps_taken;
int accel_until, decel_after;
uint32_t current_feed_rate;
uint32_t current_acceleration;
uint32_t start_feed_rate, end_feed_rate, isr_nominal_rate;
uint32_t time_accelerating, time_decelerating;
float max_xy_jerk = MAX_JERK;
float max_feedrate_mm_s[NUM_MOTORS + NUM_SERVOS];
uint8_t isr_step_multiplier = 1;

int delta0;
int over0;
long global_steps_0;
int global_step_dir_0;
#if NUM_MOTORS>1
int delta1;
int over1;
long global_steps_1;
int global_step_dir_1;
#endif
#if NUM_MOTORS>2
int delta2;
int over2;
long global_steps_2;
int global_step_dir_2;
#endif
#if NUM_MOTORS>3
int delta3;
int over3;
long global_steps_3;
int global_step_dir_3;
#endif
#if NUM_MOTORS>4
int delta4;
int over4;
long global_steps_4;
int global_step_dir_4;
#endif
#if NUM_MOTORS>5
int delta5;
int over5;
long global_steps_5;
int global_step_dir_5;
#endif

float previous_nominal_speed = 0;
float previous_safe_speed = 0;
float previous_speed[NUM_MOTORS + NUM_SERVOS];

const char *MotorNames = "LRUVWT";
const char *AxisNames = "XYZUVWT";
float maxFeedRate[NUM_MOTORS];


//------------------------------------------------------------------------------
// METHODS
//------------------------------------------------------------------------------
#ifdef ESP8266
void itr();
#endif


const int movesPlanned() {
  return SEGMOD( last_segment - current_segment );
}

// for reasons I don't understand... if i put this method in the .h file i get compile errors.
// so I put it here, which forces the externs.
FORCE_INLINE Segment *get_current_segment() {
  if (current_segment == last_segment ) return NULL;
  if (first_segment_delay > 0) {
    --first_segment_delay;
    if (movesPlanned() > 3) first_segment_delay = 0;
    return NULL;
  }
  return &line_segments[current_segment];
}


FORCE_INLINE int get_next_segment(int i) {
  return SEGMOD( i + 1 );
}


FORCE_INLINE int get_prev_segment(int i) {
  return SEGMOD( i - 1 );
}

/**
   Calculate the maximum allowable speed at this point, in order
   to reach 'target_velocity' using 'acceleration' within a given
   'distance'.
   @param acc acceleration
   @param target_velocity
   @param distance
*/
float max_speed_allowed(const float &acc, const float &target_velocity, const float &distance) {
  return sqrt( sq(target_velocity) - 2 * acc * distance );
}


/**
   set up the pins for each motor
*/
void motor_setup() {
  motors[0].step_pin        = MOTOR_0_STEP_PIN;
  motors[0].dir_pin         = MOTOR_0_DIR_PIN;
  motors[0].enable_pin      = MOTOR_0_ENABLE_PIN;
  motors[0].limit_switch_pin = MOTOR_0_LIMIT_SWITCH_PIN;
#if NUM_MOTORS>1
  motors[1].step_pin        = MOTOR_1_STEP_PIN;
  motors[1].dir_pin         = MOTOR_1_DIR_PIN;
  motors[1].enable_pin      = MOTOR_1_ENABLE_PIN;
  motors[1].limit_switch_pin = MOTOR_1_LIMIT_SWITCH_PIN;
#endif
#if NUM_MOTORS>2
  motors[2].step_pin        = MOTOR_2_STEP_PIN;
  motors[2].dir_pin         = MOTOR_2_DIR_PIN;
  motors[2].enable_pin      = MOTOR_2_ENABLE_PIN;
  motors[2].limit_switch_pin = MOTOR_2_LIMIT_SWITCH_PIN;
#endif
#if NUM_MOTORS>3
  motors[3].step_pin        = MOTOR_3_STEP_PIN;
  motors[3].dir_pin         = MOTOR_3_DIR_PIN;
  motors[3].enable_pin      = MOTOR_3_ENABLE_PIN;
  motors[3].limit_switch_pin = MOTOR_3_LIMIT_SWITCH_PIN;
#endif
#if NUM_MOTORS>4
  motors[4].step_pin        = MOTOR_4_STEP_PIN;
  motors[4].dir_pin         = MOTOR_4_DIR_PIN;
  motors[4].enable_pin      = MOTOR_4_ENABLE_PIN;
  motors[4].limit_switch_pin = MOTOR_4_LIMIT_SWITCH_PIN;
#endif
#if NUM_MOTORS>5
  motors[5].step_pin        = MOTOR_5_STEP_PIN;
  motors[5].dir_pin         = MOTOR_5_DIR_PIN;
  motors[5].enable_pin      = MOTOR_5_ENABLE_PIN;
  motors[5].limit_switch_pin = MOTOR_5_LIMIT_SWITCH_PIN;
#endif

  int i;
  for (i = 0; i < NUM_MOTORS; ++i) {
    // set the motor pin & scale
    pinMode(motors[i].step_pin, OUTPUT);
    pinMode(motors[i].dir_pin, OUTPUT);
    pinMode(motors[i].enable_pin, OUTPUT);

    // set the switch pin
    pinMode(motors[i].limit_switch_pin, INPUT);
    digitalWrite(motors[i].limit_switch_pin, HIGH);
  }

  long steps[NUM_MOTORS + NUM_SERVOS];
  memset(steps, 0, (NUM_MOTORS + NUM_SERVOS)*sizeof(long));

  for (i = 0; i < NUM_MOTORS; ++i) {
    max_feedrate_mm_s[i] = MAX_FEEDRATE;
  }
  for (i = NUM_MOTORS; i < NUM_MOTORS + NUM_SERVOS; ++i) {
    max_feedrate_mm_s[i] = MAX_FEEDRATE;
  }

  motor_set_step_count(steps);

  // setup servos
#if NUM_SERVOS>0
#ifdef ESP8266
  pinMode(SERVO0_PIN, OUTPUT);
#else
  servos[0].attach(SERVO0_PIN);
#endif  // ESP8266
#endif

#if NUM_SERVOS>1
  servos[1].attach(SERVO1_PIN);
#endif
#if NUM_SERVOS>2
  servos[2].attach(SERVO2_PIN);
#endif
#if NUM_SERVOS>3
  servos[3].attach(SERVO3_PIN);
#endif
#if NUM_SERVOS>4
  servos[4].attach(SERVO4_PIN);
#endif

  current_segment = 0;
  last_segment = 0;
  Segment &old_seg = line_segments[get_prev_segment(last_segment)];
  old_seg.a[0].step_count = 0;
#if NUM_MOTORS>1
  old_seg.a[1].step_count = 0;
#endif
#if NUM_MOTORS>2
  old_seg.a[2].step_count = 0;
#endif
#if NUM_MOTORS>3
  old_seg.a[3].step_count = 0;
#endif
#if NUM_MOTORS>4
  old_seg.a[4].step_count = 0;
#endif
#if NUM_MOTORS>5
  old_seg.a[5].step_count = 0;
#endif

#if NUM_SERVOS>0
  old_seg.a[NUM_MOTORS].step_count = 0;
#endif

  working_seg = NULL;
  first_segment_delay = 0;

  // disable global interrupts
  noInterrupts();
#ifdef ESP8266
  timer0_isr_init();
  timer0_attachInterrupt(itr);
  CLOCK_ADJUST(2000);
#else
  // set entire TCCR1A register to 0
  TCCR1A = 0;
  // set the overflow clock to 0
  TCNT1  = 0;
  // set compare match register to desired timer count
  OCR1A = 2000;  // 1ms
  // turn on CTC mode
  TCCR1B = (1 << WGM12);
  // Set 8x prescaler
  TCCR1B = (TCCR1B & ~(0x07 << CS10)) | (2 << CS10);
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
#endif  // ESP8266

  interrupts();  // enable global interrupts
}


// turn on power to the motors (make them immobile)
void motor_engage() {
  int i;
  for (i = 0; i < NUM_MOTORS; ++i) {
    digitalWrite(motors[i].enable_pin, LOW);
  }
  /*
    #if MACHINE_STYLE == ARM6
    // DM320T drivers want high for enabled
    digitalWrite(motors[4].enable_pin,HIGH);
    digitalWrite(motors[5].enable_pin,HIGH);
    #endif*/
}


// turn off power to the motors (make them move freely)
void motor_disengage() {
  int i;
  for (i = 0; i < NUM_MOTORS; ++i) {
    digitalWrite(motors[i].enable_pin, HIGH);
  }/*
  #if MACHINE_STYLE == ARM6
  // DM320T drivers want low for disabled
  digitalWrite(motors[4].enable_pin,LOW);
  digitalWrite(motors[5].enable_pin,LOW);
  #endif*/
}


// Change pen state.
void setPenAngle(int arg0) {
#if NUM_AXIES>=3
  if (arg0 < axies[2].limitMin) arg0 = axies[2].limitMin;
  if (arg0 > axies[2].limitMax) arg0 = axies[2].limitMax;

  axies[2].pos = arg0;
#endif  // NUM_AXIES>=3

#if NUM_SERVOS>0
// this is commented out because compiler segfault for unknown reasons.
//#ifndef ESP8266
//  servos[0].write(arg0);
//#else
  analogWrite(SERVO0_PIN, arg0);
//#endif  // ESP8266
#endif // NUM_SERVOS>0
}




void recalculate_reverse_kernel(Segment *const current, const Segment *next) {
  if (current == NULL) return;

  const float entry_speed_max2 = current->entry_speed_max;
  if (current->entry_speed != entry_speed_max2 || (next && next->recalculate_flag) ) {
    // If nominal length true, max junction speed is guaranteed to be reached. Only compute
    // for max allowable speed if block is decelerating and nominal length is false.
    const float new_entry_speed = current->nominal_length_flag
                                  ? entry_speed_max2
                                  : min( entry_speed_max2, max_speed_allowed(-current->acceleration, (next ? next->entry_speed : MIN_FEEDRATE), current->distance) );

    if (current->entry_speed != new_entry_speed ) {
      current->entry_speed = new_entry_speed;
      current->recalculate_flag = true;
    }
  }
}


void recalculate_reverse() {
  if (last_segment == current_segment) return;

  int s = get_prev_segment(last_segment);

  Segment *current, *next = NULL;
  while (s != current_segment) {
    current = &line_segments[s];

    recalculate_reverse_kernel(current, next);
    next = current;
    s = get_prev_segment(s);
  }
}


void recalculate_forward_kernel(const Segment *prev, Segment *const current) {
  if (prev == NULL) return;

  // If the previous block is an acceleration block, but it is not long enough to complete the
  // full speed change within the block, we need to adjust the entry speed accordingly. Entry
  // speeds have already been reset, maximized, and reverse planned by reverse planner.
  // If nominal length is true, max junction speed is guaranteed to be reached. No need to recheck.
  if (!prev->nominal_length_flag && prev->entry_speed < current->entry_speed) {
    const float new_entry_speed2 = max_speed_allowed(-prev->acceleration, prev->entry_speed, prev->distance);
    // Check for junction speed change
    if (new_entry_speed2 < current->entry_speed) {
      current->recalculate_flag = true;
    } else {
      current->entry_speed = new_entry_speed2;
    }
  }
}


void recalculate_forward() {
  int s = current_segment;

  Segment *previous = NULL, *current;
  while (s != last_segment) {
    current = &line_segments[s];
    recalculate_forward_kernel(previous, current);
    previous = current;
    s = get_next_segment(s);
  }
}


float estimate_acceleration_distance(const float &initial_rate, const float &target_rate, const float &accel) {
  if (accel == 0) return 0; // accel was 0, set acceleration distance to 0
  return (sq(target_rate) - sq(initial_rate)) / (accel * 2);
}


int intersection_distance(const float &start_rate, const float &end_rate, const float &accel, const float &distance) {
  return ( 2.0 * accel * distance - sq(start_rate) + sq(end_rate) ) / (4.0 * accel);
}


void segment_update_trapezoid(Segment *s, const float &entry_factor, const float &exit_factor) {
  uint32_t intial_rate = ceil(s->nominal_rate * entry_factor);
  uint32_t final_rate  = ceil(s->nominal_rate * exit_factor );

  if (intial_rate < MIN_STEP_RATE) intial_rate = MIN_STEP_RATE;
  if (final_rate  < MIN_STEP_RATE) final_rate  = MIN_STEP_RATE;

  const int32_t accel = s->acceleration_steps_per_s2;
  int32_t accelerate_steps =  ceil( estimate_acceleration_distance(intial_rate    , s->nominal_rate,  accel) );
  int32_t decelerate_steps = floor( estimate_acceleration_distance(s->nominal_rate, final_rate     , -accel) );
  int32_t plateau_steps = s->steps_total - accelerate_steps - decelerate_steps;
  if (plateau_steps < 0) {
    accelerate_steps = ceil( intersection_distance( intial_rate, final_rate, accel, s->steps_total ) );
    accelerate_steps = min( (uint32_t)max( accelerate_steps, 0 ), s->steps_total );
    plateau_steps = 0;
  }
  CRITICAL_SECTION_START
  /*
    decelerate_steps = s->steps_total - plateau_steps - accelerate_steps;
    Serial.print("t entry_factor=");  Serial.print(entry_factor);
    Serial.print(" exit_factor=");  Serial.print(exit_factor);
    Serial.print(" accel=");  Serial.print(accel);
    Serial.print(" intial_rate=");  Serial.print(intial_rate);
    Serial.print(" nominal_rate=");  Serial.print(s->nominal_rate);
    Serial.print(" final_rate=");  Serial.print(final_rate);
    Serial.print(" steps_total=");  Serial.print(s->steps_total);
    Serial.print(" accelerate_steps=");  Serial.print(accelerate_steps);
    Serial.print(" plateau_steps=");  Serial.print(plateau_steps);
    Serial.print(" decelerate_steps=");  Serial.print(decelerate_steps);
    Serial.println();
  */
  if (!s->busy) {
    s->accel_until = accelerate_steps;
    s->decel_after = accelerate_steps + plateau_steps;
    s->initial_rate = intial_rate;
    s->final_rate  = final_rate;
  }
  CRITICAL_SECTION_END
}


void recalculate_trapezoids() {
  int s = current_segment;
  Segment *current = NULL;
  Segment *next = NULL;

  float current_entry_speed = 0, next_entry_speed = 0;

  while (s != last_segment) {
    next = &line_segments[s];
    next_entry_speed = next->entry_speed;
    if (current) {
      // Recalculate if current block entry or exit junction speed has changed.
      if ( current->recalculate_flag || next->recalculate_flag ) {
        if (!current->busy) {
          // NOTE: Entry and exit factors always > 0 by all previous logic operations.
          const float inom = 1.0 / current->nominal_speed;
          segment_update_trapezoid(current, current_entry_speed * inom, next_entry_speed * inom);
        }
      }
      current->recalculate_flag = false; // Reset current only to ensure next trapezoid is computed
    }
    s = get_next_segment(s);
    current_entry_speed = next_entry_speed;
    current = next;
  }

  // Last/newest block in buffer. Make sure the last block always ends motion.
  if (next != NULL) {
    const float inom = 1.0 / next->nominal_speed;
    segment_update_trapezoid(next, next_entry_speed * inom, MIN_FEEDRATE * inom);
    next->recalculate_flag = false;
  }
}


void describe_segments() {
  CRITICAL_SECTION_START
  /**/
  Serial.println("A = index");
  Serial.println("B = distance");
  Serial.println("C = acceleration");

  Serial.println("D = entry_speed");
  Serial.println("E = nominal_speed");
  Serial.println("F = entry_speed_max");

  Serial.println("G = entry rate");
  Serial.println("H = nominal rate");
  Serial.println("I = exit rate");

  Serial.println("J = accel_until");
  Serial.println("K = coast steps");
  Serial.println("L = decel steps");

  Serial.println("O = nominal?");
  Serial.println("P = recalculate?");
  Serial.println("Q = busy?");/**/
  Serial.println("\nA\tB\tC\tD\tE\tF\t[G\tH\tI]\t[J\tK\tL]\tO\tP\tQ");
  Serial.println("---------------------------------------------------------------------------------------------------------------------------");

  int s = current_segment;
  while (s != last_segment) {
    Segment *next = &line_segments[s];
    int coast = next->decel_after - next->accel_until;
    int decel = next->steps_total - next->decel_after;
    Serial.print(s);
    Serial.print(F("\t"));   Serial.print(next->distance);
    Serial.print(F("\t"));   Serial.print(next->acceleration);

    Serial.print(F("\t"));   Serial.print(next->entry_speed);
    Serial.print(F("\t"));   Serial.print(next->nominal_speed);
    Serial.print(F("\t"));   Serial.print(next->entry_speed_max);

    Serial.print(F("\t"));   Serial.print(next->initial_rate);
    Serial.print(F("\t"));   Serial.print(next->nominal_rate);
    Serial.print(F("\t"));   Serial.print(next->final_rate);

    Serial.print(F("\t"));   Serial.print(next->accel_until);
    Serial.print(F("\t"));   Serial.print(coast);
    Serial.print(F("\t"));   Serial.print(decel);
    //Serial.print(F("\t"));   Serial.print(next->steps_total);
    //Serial.print(F("\t"));   Serial.print(next->steps_taken);

    Serial.print(F("\t"));   Serial.print(next->nominal_length_flag != 0 ? 'Y' : 'N');
    Serial.print(F("\t"));   Serial.print(next->recalculate_flag != 0 ? 'Y' : 'N');
    Serial.print(F("\t"));   Serial.print(next->busy != 0 ? 'Y' : 'N');
    Serial.println();
    s = get_next_segment(s);
  }
  CRITICAL_SECTION_END
}


void recalculate_acceleration() {
  recalculate_reverse();
  recalculate_forward();
  recalculate_trapezoids();

  //Serial.println("**FINISHED**");
  //describe_segments();
}


void motor_set_step_count(long *a) {
  wait_for_empty_segment_buffer();

  for (int i = 0; i < NUM_MOTORS + NUM_SERVOS; ++i) {
    previous_speed[i] = 0;
  }

  Segment &old_seg = line_segments[get_prev_segment(last_segment)];
  old_seg.a[0].step_count = a[0];
#if NUM_MOTORS>1
  old_seg.a[1].step_count = a[1];
#endif
#if NUM_MOTORS>2
  old_seg.a[2].step_count = a[2];
#endif
#if NUM_MOTORS>3
  old_seg.a[3].step_count = a[3];
#endif
#if NUM_MOTORS>4
  old_seg.a[4].step_count = a[4];
#endif
#if NUM_MOTORS>5
  old_seg.a[5].step_count = a[5];
#endif
#if NUM_SERVOS>0
  old_seg.a[NUM_MOTORS].step_count = a[NUM_MOTORS];
#endif

  global_steps_0 = 0;
#if NUM_MOTORS>1
  global_steps_1 = 0;
#endif
#if NUM_MOTORS>2
  global_steps_2 = 0;
#endif
#if NUM_MOTORS>3
  global_steps_3 = 0;
#endif
#if NUM_MOTORS>4
  global_steps_4 = 0;
#endif
#if NUM_MOTORS>5
  global_steps_5 = 0;
#endif
}


/**
   Step one motor one time in the currently set direction.
   @input newx the destination x position
   @input newy the destination y position
 **/
void motor_onestep(int motor) {
#ifdef VERBOSE
  Serial.print(motorNames[motor]);
#endif

  digitalWrite(motors[motor].step_pin, HIGH);
  digitalWrite(motors[motor].step_pin, LOW);
}


/**
   Set the clock 2 timer frequency.
   @input desired_freq_hz the desired frequency
   Different clock sources can be selected for each timer independently.
   To calculate the timer frequency (for example 2Hz using timer1) you will need:
*/
FORCE_INLINE unsigned short calc_timer(uint32_t desired_freq_hz, uint8_t*loops) {
  if ( desired_freq_hz > CLOCK_MAX_STEP_FREQUENCY ) desired_freq_hz = CLOCK_MAX_STEP_FREQUENCY;
  if ( desired_freq_hz < CLOCK_MIN_STEP_FREQUENCY ) desired_freq_hz = CLOCK_MIN_STEP_FREQUENCY;
  //desired_freq_hz-=CLOCK_MIN_STEP_FREQUENCY;

  uint8_t step_multiplier = 1;
  if ( desired_freq_hz > 20000 ) {
    step_multiplier = 4;
    desired_freq_hz >>= 2;
  } else if ( desired_freq_hz > 10000 ) {
    step_multiplier = 2;
    desired_freq_hz >>= 1;
  }
  *loops = step_multiplier;

  long counter_value = ( CLOCK_FREQ >> 3 ) / desired_freq_hz;
  if ( counter_value >= MAX_COUNTER ) {
    counter_value = MAX_COUNTER - 1;
  } else if ( counter_value < 100 ) {
    counter_value = 100;
  }

  return counter_value;
}


#define A(CODE) " " CODE "\n\t"

// intRes = longIn1 * longIn2 >> 24
// uses:
// A[tmp] to store 0
// B[tmp] to store bits 16-23 of the 48bit result. The top bit is used to round the two byte result.
// note that the lower two bytes and the upper byte of the 48bit result are not calculated.
// this can cause the result to be out by one as the lower bytes may cause carries into the upper ones.
// B A are bits 24-39 and are the returned value
// C B A is longIn1
// D C B A is longIn2
//
static FORCE_INLINE uint16_t MultiU24X32toH16(uint32_t longIn1, uint32_t longIn2) {
#ifdef ESP8266
  uint16_t intRes = longIn1 * longIn2 >> 24;
#else // ESP8266
  register uint8_t tmp1;
  register uint8_t tmp2;
  register uint16_t intRes;
  __asm__ __volatile__(
    A("clr %[tmp1]")
    A("mul %A[longIn1], %B[longIn2]")
    A("mov %[tmp2], r1")
    A("mul %B[longIn1], %C[longIn2]")
    A("movw %A[intRes], r0")
    A("mul %C[longIn1], %C[longIn2]")
    A("add %B[intRes], r0")
    A("mul %C[longIn1], %B[longIn2]")
    A("add %A[intRes], r0")
    A("adc %B[intRes], r1")
    A("mul %A[longIn1], %C[longIn2]")
    A("add %[tmp2], r0")
    A("adc %A[intRes], r1")
    A("adc %B[intRes], %[tmp1]")
    A("mul %B[longIn1], %B[longIn2]")
    A("add %[tmp2], r0")
    A("adc %A[intRes], r1")
    A("adc %B[intRes], %[tmp1]")
    A("mul %C[longIn1], %A[longIn2]")
    A("add %[tmp2], r0")
    A("adc %A[intRes], r1")
    A("adc %B[intRes], %[tmp1]")
    A("mul %B[longIn1], %A[longIn2]")
    A("add %[tmp2], r1")
    A("adc %A[intRes], %[tmp1]")
    A("adc %B[intRes], %[tmp1]")
    A("lsr %[tmp2]")
    A("adc %A[intRes], %[tmp1]")
    A("adc %B[intRes], %[tmp1]")
    A("mul %D[longIn2], %A[longIn1]")
    A("add %A[intRes], r0")
    A("adc %B[intRes], r1")
    A("mul %D[longIn2], %B[longIn1]")
    A("add %B[intRes], r0")
    A("clr r1")
    : [intRes] "=&r" (intRes),
    [tmp1] "=&r" (tmp1),
    [tmp2] "=&r" (tmp2)
    : [longIn1] "d" (longIn1),
    [longIn2] "d" (longIn2)
    : "cc"
  );
#endif // ESP8266
  return intRes;
}

/**
   Process all line segments in the ring buffer.  Uses bresenham's line algorithm to move all motors.
*/

#ifdef ESP8266
void itr() {
#else
ISR(TIMER1_COMPA_vect) {
#endif
  // segment buffer empty? do nothing
  if ( working_seg == NULL ) {
    working_seg = get_current_segment();

    if ( working_seg != NULL ) {
      // New segment!
      working_seg->busy = true;

      // set the direction pins
      digitalWrite( MOTOR_0_DIR_PIN, working_seg->a[0].dir );
      global_step_dir_0 = (working_seg->a[0].dir == HIGH) ? 1 : -1;

#if NUM_MOTORS>1
      digitalWrite( MOTOR_1_DIR_PIN, working_seg->a[1].dir );
      global_step_dir_1 = (working_seg->a[1].dir == HIGH) ? 1 : -1;
#endif
#if NUM_MOTORS>2
      digitalWrite( MOTOR_2_DIR_PIN, working_seg->a[2].dir );
      global_step_dir_2 = (working_seg->a[2].dir == HIGH) ? 1 : -1;
#endif
#if NUM_MOTORS>3
      digitalWrite( MOTOR_3_DIR_PIN, working_seg->a[3].dir );
      global_step_dir_3 = (working_seg->a[3].dir == HIGH) ? 1 : -1;
#endif
#if NUM_MOTORS>4
      digitalWrite( MOTOR_4_DIR_PIN, working_seg->a[4].dir );
      global_step_dir_4 = (working_seg->a[4].dir == HIGH) ? 1 : -1;
#endif
#if NUM_MOTORS>5
      digitalWrite( MOTOR_5_DIR_PIN, working_seg->a[5].dir );
      global_step_dir_5 = (working_seg->a[5].dir == HIGH) ? 1 : -1;
#endif

#if NUM_SERVOS>0
#ifdef ESP8266
      analogWrite(SERVO0_PIN, working_seg->a[NUM_MOTORS].step_count);
#else
      servos[0].write(working_seg->a[NUM_MOTORS].step_count);
#endif  // ESP8266
#endif  // NUM_SERVOS>0

      start_feed_rate = working_seg->initial_rate;
      end_feed_rate = working_seg->final_rate;
      current_feed_rate = start_feed_rate;
      current_acceleration = working_seg->acceleration_rate;
      accel_until = working_seg->accel_until;
      decel_after = working_seg->decel_after;
      time_accelerating = 0;
      time_decelerating = 0;
      isr_nominal_rate = 0;

      // defererencing some data so the loop runs faster.
      steps_total = working_seg->steps_total;
      steps_taken = 0;
      delta0 = working_seg->a[0].absdelta;      over0 = -(steps_total >> 1);
#if NUM_MOTORS>1
      delta1 = working_seg->a[1].absdelta;      over1 = -(steps_total >> 1);
#endif
#if NUM_MOTORS>2
      delta2 = working_seg->a[2].absdelta;      over2 = -(steps_total >> 1);
#endif
#if NUM_MOTORS>3
      delta3 = working_seg->a[3].absdelta;      over3 = -(steps_total >> 1);
#endif
#if NUM_MOTORS>4
      delta4 = working_seg->a[4].absdelta;      over4 = -(steps_total >> 1);
#endif
#if NUM_MOTORS>5
      delta5 = working_seg->a[5].absdelta;      over5 = -(steps_total >> 1);
#endif
      return;
    } else {
      CLOCK_ADJUST(2000); // wait 1ms
      return;
    }
  }

  if ( working_seg != NULL ) {
    // move each axis
    for (uint8_t i = 0; i < isr_step_multiplier; ++i) {
      over0 += delta0;
      if (over0 > 0) digitalWrite(MOTOR_0_STEP_PIN, LOW);
#if NUM_MOTORS>1
      over1 += delta1;
      if (over1 > 0) digitalWrite(MOTOR_1_STEP_PIN, LOW);
#endif
#if NUM_MOTORS>2
      over2 += delta2;
      if (over2 > 0) digitalWrite(MOTOR_2_STEP_PIN, LOW);
#endif
#if NUM_MOTORS>3
      over3 += delta3;
      if (over3 > 0) digitalWrite(MOTOR_3_STEP_PIN, LOW);
#endif
#if NUM_MOTORS>4
      over4 += delta4;
      if (over4 > 0) digitalWrite(MOTOR_4_STEP_PIN, LOW);
#endif
#if NUM_MOTORS>5
      over5 += delta5;
      if (over5 > 0) digitalWrite(MOTOR_5_STEP_PIN, LOW);
#endif
      // now that the pins have had a moment to settle, do the second half of the steps.
      // M0
      if (over0 > 0) {
        over0 -= steps_total;
        global_steps_0 += global_step_dir_0;
        digitalWrite(MOTOR_0_STEP_PIN, HIGH);
      }
#if NUM_MOTORS>1
      // M1
      if (over1 > 0) {
        over1 -= steps_total;
        global_steps_1 += global_step_dir_1;
        digitalWrite(MOTOR_1_STEP_PIN, HIGH);
      }
#endif
#if NUM_MOTORS>2
      // M2
      if (over2 > 0) {
        over2 -= steps_total;
        global_steps_2 += global_step_dir_2;
        digitalWrite(MOTOR_2_STEP_PIN, HIGH);
      }
#endif
#if NUM_MOTORS>3
      // M3
      if (over3 > 0) {
        over3 -= steps_total;
        global_steps_3 += global_step_dir_3;
        digitalWrite(MOTOR_3_STEP_PIN, HIGH);
      }
#endif
#if NUM_MOTORS>4
      // M4
      if (over4 > 0) {
        over4 -= steps_total;
        global_steps_4 += global_step_dir_4;
        digitalWrite(MOTOR_4_STEP_PIN, HIGH);
      }
#endif
#if NUM_MOTORS>5
      // M5
      if (over5 > 0) {
        over5 -= steps_total;
        global_steps_5 += global_step_dir_5;
        digitalWrite(MOTOR_5_STEP_PIN, HIGH);
      }
#endif

      // make a step
      steps_taken++;
      if (steps_taken >= steps_total) break;
    }


    // Is this segment done?
    if ( steps_taken >= steps_total ) {
      // Move on to next segment without wasting an interrupt tick.
      working_seg = NULL;
      current_segment = get_next_segment(current_segment);
      return;
    }

    // accel
    uint32_t interval;
    if ( steps_taken <= accel_until ) {
      current_feed_rate = start_feed_rate + MultiU24X32toH16( time_accelerating, current_acceleration );
      if (current_feed_rate > working_seg->nominal_rate) {
        current_feed_rate = working_seg->nominal_rate;
      }
      interval = calc_timer(current_feed_rate, &isr_step_multiplier);
      time_accelerating += interval;
      CLOCK_ADJUST(interval);
      /*
        Serial.print("A\t");
        Serial.print(current_feed_rate);
        Serial.print("\t");
        Serial.println(isr_step_multiplier);//*/
      /*
        Serial.print("A >> ");   Serial.print(interval);
        Serial.print("\t");      Serial.print(isr_step_multiplier);
        Serial.print("\t");      Serial.print(current_feed_rate);
        Serial.print(" = ");     Serial.print(start_feed_rate);
        Serial.print(" + ");     Serial.print(current_acceleration);
        Serial.print(" * ");     Serial.print(time_accelerating);
        Serial.println();//*/
    } else if ( steps_taken > decel_after ) {
      uint32_t end_feed_rate = current_feed_rate - MultiU24X32toH16( time_decelerating, current_acceleration );
      if ( end_feed_rate < working_seg->final_rate ) {
        end_feed_rate = working_seg->final_rate;
      }
      interval = calc_timer(end_feed_rate, &isr_step_multiplier);
      time_decelerating += interval;
      CLOCK_ADJUST(interval);
      /*
        Serial.print("D\t");
        Serial.print(end_feed_rate);
        Serial.print("\t");
        Serial.println(isr_step_multiplier);//*/
      /*
        Serial.print("D >> ");  Serial.print(interval);
        Serial.print("\t");     Serial.print(isr_step_multiplier);
        Serial.print("\t");     Serial.print(end_feed_rate);
        Serial.print(" = ");     Serial.print(current_feed_rate);
        Serial.print(" - ");     Serial.print(current_acceleration);
        Serial.print(" * ");     Serial.print(time_decelerating);
        Serial.println();//*/
    } else {
      if (isr_nominal_rate == 0) {
        isr_nominal_rate = calc_timer(working_seg->nominal_rate, &isr_step_multiplier);
      }
      CLOCK_ADJUST(isr_nominal_rate);
      /*
        Serial.print("C\t");
        Serial.print(working_seg->nominal_rate);
        Serial.print("\t");
        Serial.println(isr_step_multiplier);//*/
      /*
        Serial.print("C >> ");  Serial.println(working_seg->nominal_rate);
        //Serial.print("\t");  Serial.print(interval);
        //Serial.print("\t");     Serial.print(isr_step_multiplier);
        Serial.print("\t");     Serial.print(current_feed_rate);
        Serial.println();//*/
    }
#ifndef ESP8266
    OCR1A = (OCR1A < (TCNT1 + 16)) ? (TCNT1 + 16) : OCR1A;
#endif // ESP8266
  }
}


/**
   @return 1 if buffer is full, 0 if it is not.
*/
char segment_buffer_full() {
  int next_segment = get_next_segment(last_segment);
  return (next_segment == current_segment);
}


/**
   Translate the XYZ through the IK to get the number of motor steps and move the motors.
   Uses bresenham's line algorithm to move both motors
   @input pos NUM_AXIES floats describing destination coordinates
   @input new_feed_rate speed to travel along arc
*/
void motor_line(const float * const target_position, float &fr_mm_s) {
  long steps[NUM_MOTORS + NUM_SERVOS];

  // convert from the cartesian position to the motor steps
  IK(target_position, steps);

  float distance_mm = 0;

  // record the new target position & feed rate for the next movement.
  int i;
  for (i = 0; i < NUM_AXIES; ++i) {
    distance_mm += sq(target_position[i] - axies[i].pos);
    axies[i].pos = target_position[i];
  }
  distance_mm = sqrt( distance_mm );
  feed_rate = fr_mm_s;

  // get the next available spot in the segment buffer
  int next_segment = get_next_segment(last_segment);
  while ( next_segment == current_segment ) {
    // the segment buffer is full, we are way ahead of the motion system.  wait here.
    delay(1);
  }

  int prev_segment = get_prev_segment(last_segment);
  Segment &new_seg = line_segments[last_segment];
  Segment &old_seg = line_segments[prev_segment];

  // use LCD to adjust speed while drawing
#ifdef HAS_LCD
  fr_mm_s *= (float)speed_adjust * 0.01f;
#endif

  new_seg.busy = false;
  new_seg.distance = distance_mm;
  float inverse_distance_mm = 1.0 / new_seg.distance;
  float inverse_secs = fr_mm_s * inverse_distance_mm;

  // The axis that has the most steps will control the overall acceleration as per bresenham's algorithm.
  new_seg.steps_total = 0;
  for (i = 0; i < NUM_MOTORS + NUM_SERVOS; ++i) {
    new_seg.a[i].step_count = steps[i];
    new_seg.a[i].delta_steps = steps[i] - old_seg.a[i].step_count;
    new_seg.a[i].delta_mm = new_seg.a[i].delta_steps * THREAD_PER_STEP;
    new_seg.a[i].dir = ( new_seg.a[i].delta_steps < 0 ? HIGH : LOW );
    new_seg.a[i].absdelta = abs(new_seg.a[i].delta_steps);
    if ( new_seg.steps_total < new_seg.a[i].absdelta ) {
      new_seg.steps_total = new_seg.a[i].absdelta;
    }
  }

  // No steps?  No work!  Stop now.
  if ( new_seg.steps_total == 0 ) return;

  new_seg.nominal_speed = new_seg.distance * inverse_secs;
  new_seg.nominal_rate = ceil(new_seg.steps_total * inverse_secs);

  // Calculate the the speed limit for each axis
  float current_speed[NUM_MOTORS + NUM_SERVOS], speed_factor = 1.0;
  for (i = 0; i < NUM_MOTORS + NUM_SERVOS; ++i) {
    current_speed[i] = new_seg.a[i].delta_mm * inverse_secs;
    const float cs = fabs(current_speed[i]);
    if (cs > max_feedrate_mm_s[i]) speed_factor = min (speed_factor, max_feedrate_mm_s[i] / cs);
    //if (cs > MAX_FEEDRATE) speed_factor = min (speed_factor, MAX_FEEDRATE / cs);
  }

  if (speed_factor < 1.0) {
    for (i = 0; i < NUM_MOTORS + NUM_SERVOS; ++i) {
      current_speed[i] *= speed_factor;
    }
    new_seg.nominal_speed *= speed_factor;
    new_seg.nominal_rate *= speed_factor;
  }

  const float steps_per_mm = new_seg.steps_total * inverse_distance_mm;
  uint32_t accel = ceil( acceleration * steps_per_mm );

  const float max_acceleration_steps_per_s2 = acceleration * steps_per_mm;
  for (i = 0; i < NUM_MOTORS + NUM_SERVOS; ++i) {
    if (new_seg.a[i].delta_steps && max_acceleration_steps_per_s2 < accel) {
      const float comp = (float)max_acceleration_steps_per_s2 * (float)new_seg.steps_total;
      if ((float)accel * (float)new_seg.a[i].delta_steps > comp ) {
        accel = comp / (float)new_seg.a[i].delta_steps;
      }
    }
  }
  new_seg.acceleration_steps_per_s2 = accel;
  new_seg.acceleration = accel / steps_per_mm;
  new_seg.acceleration_rate = (uint32_t)(accel * (4096.0f * 4096.0f / (TIMER_RATE)));
  new_seg.steps_taken = 0;

  // TODO explain this
  float safe_speed = new_seg.nominal_speed;
  char limited = 0;
  for (i = 0; i < NUM_MOTORS + NUM_SERVOS; ++i) {
    const float jerk = fabs(current_speed[i]), maxj = max_xy_jerk;
    if (jerk > maxj) {
      if (limited) {
        // TODO explain this
        const float mjerk = maxj * new_seg.nominal_speed;
        if (jerk * safe_speed > mjerk) safe_speed = mjerk / jerk;
      } else {
        ++limited;
        safe_speed *= maxj / jerk;
      }
    }
  }

  // what is the maximum starting speed for this segment?
  float vmax_junction = MIN_FEEDRATE;

  int movesQueued = movesPlanned();
  if (movesQueued > 0 && previous_safe_speed > 0.0001) {
    // Estimate a maximum velocity allowed at a joint of two successive segments.
    // If this maximum velocity allowed is lower than the minimum of the entry / exit safe velocities,
    // then the machine is not coasting anymore and the safe entry / exit velocities shall be used.

    // The junction velocity will be shared between successive segments. Limit the junction velocity to their minimum.
    // Pick the smaller of the nominal speeds. Higher speed shall not be achieved at the junction during coasting.
    vmax_junction = min(new_seg.nominal_speed, previous_nominal_speed);
    const float smaller_speed_factor = vmax_junction / previous_nominal_speed;
    // Factor to multiply the previous / current nominal velocities to get componentwise limited velocities.
    float v_factor = 1.0f;
    limited = 0;
    // Now limit the jerk in all axes.
    for (i = 0; i < NUM_MOTORS + NUM_SERVOS; ++i) {
      // Limit an axis. We have to differentiate: coasting, reversal of an axis, full stop.
      float v_exit = previous_speed[i] * smaller_speed_factor;
      float v_entry = current_speed[i];
      if (limited) {
        v_exit *= v_factor;
        v_entry *= v_factor;
      }

      // Calculate jerk depending on whether the axis is coasting in the same direction or reversing.
      const float jerk = (v_exit > v_entry)
                         ? //                            coasting             axis reversal
                         ( (v_entry > 0 || v_exit < 0) ? (v_exit - v_entry) : max(v_exit, -v_entry) )
                         : // v_exit <= v_entry          coasting             axis reversal
                         ( (v_entry < 0 || v_exit > 0) ? (v_entry - v_exit) : max(-v_exit, v_entry) );

      if (jerk > max_xy_jerk) {
        v_factor *= max_xy_jerk / jerk;
        ++limited;
      }
    }
    if (limited) vmax_junction *= v_factor;
    // Now the transition velocity is known, which maximizes the shared exit / entry velocity while
    // respecting the jerk factors, it may be possible, that applying separate safe exit / entry velocities will achieve faster prints.
    const float vmax_junction_threshold = vmax_junction * 0.99f;
    if (previous_safe_speed > vmax_junction_threshold && safe_speed > vmax_junction_threshold) {
      // Not coasting. The machine will stop and start the movements anyway,
      // better to start the segment from start.
      //SBI(new_seg.flag, BLOCK_BIT_START_FROM_FULL_HALT);
      vmax_junction = safe_speed;
    }
  }

  float allowable_speed = max_speed_allowed(-new_seg.acceleration, MIN_FEEDRATE, new_seg.distance);

#if NUM_SERVOS>0
  // come to a stop for entering or exiting a Z move
  //if( new_seg.a[NUM_SERVOS].delta_steps != 0 || old_seg.a[NUM_SERVOS].delta_steps != 0 ) allowable_speed = MIN_FEEDRATE;
#endif

  new_seg.entry_speed_max = vmax_junction;
  new_seg.entry_speed = min(vmax_junction, allowable_speed);
  new_seg.nominal_length_flag = ( allowable_speed >= new_seg.nominal_speed );
  new_seg.recalculate_flag = true;

  previous_nominal_speed = new_seg.nominal_speed;
  previous_safe_speed = safe_speed;
  for (i = 0; i < NUM_MOTORS + NUM_SERVOS; ++i) {
    previous_speed[i] = current_speed[i];
  }

  // when should we accelerate and decelerate in this segment?
  segment_update_trapezoid(&new_seg, new_seg.entry_speed / new_seg.nominal_speed, (float)MIN_FEEDRATE / new_seg.nominal_speed);

  if (current_segment == last_segment ) {
    first_segment_delay = BLOCK_DELAY_FOR_1ST_MOVE;
  }
  last_segment = next_segment;

  recalculate_acceleration();
  /*
    Serial.print("distance=");  Serial.println(new_seg.distance);
    Serial.print("acceleration original=");  Serial.println(acceleration);
    Serial.print("nominal_speed=");  Serial.println(new_seg.nominal_speed);

    Serial.print("inverse_distance_mm=");  Serial.println(inverse_distance_mm);
    Serial.print("inverse_secs=");  Serial.println(inverse_secs);
    Serial.print("nominal_rate=");  Serial.println(new_seg.nominal_rate);
    Serial.print("delta_mm=");
    for (i = 0; i < NUM_MOTORS + NUM_SERVOS; ++i) {
    if (i > 0) Serial.print(", ");
    Serial.print(new_seg.a[i].delta_mm);
    }
    Serial.println();
    Serial.print("speed_factor=");  Serial.println(speed_factor);
    Serial.print("steps_per_mm=");  Serial.println(steps_per_mm);
    Serial.print("accel=");  Serial.println(accel);
    Serial.print("acceleration_steps_per_s2=");  Serial.println(new_seg.acceleration_steps_per_s2);
    Serial.print("acceleration=");  Serial.println(new_seg.acceleration);
    Serial.print("limited=");  Serial.println(limited, DEC);
    Serial.print("nominal_speed=");  Serial.println(new_seg.nominal_speed);
    Serial.print("vmax_junction=");  Serial.println(vmax_junction);
    Serial.print("allowable_speed=");  Serial.println(allowable_speed);
    Serial.print("safe_speed=");  Serial.println(safe_speed);
    //*/
}


void wait_for_empty_segment_buffer() {
  while ( current_segment != last_segment );
}
