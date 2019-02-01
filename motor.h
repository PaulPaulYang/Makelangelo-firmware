#ifndef MOTOR_H
#define MOTOR_H
//------------------------------------------------------------------------------
// Makelangelo - firmware for various robot kinematic models
// dan@marginallycelver.com 2013-12-26
// Please see http://www.github.com/MarginallyClever/makelangeloFirmware for more information.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// CONSTANTS
//------------------------------------------------------------------------------


typedef struct {
  int step_pin;
  int dir_pin;
  int enable_pin;
  int limit_switch_pin;
} Motor;


// for line()
typedef struct {
  long step_count;
  long delta;  // number of steps to move
  long absdelta;
  int dir;
  float delta_normalized;
} SegmentAxis;


typedef struct {
  SegmentAxis a[NUM_MOTORS+NUM_SERVOS];

  float distance;  // mm
  int steps_total;  // steps

  int steps_taken;  // steps
  int accel_until;  // steps
  int decel_after;  // steps

  float acceleration;  // acceleration mm/sec^2
  unsigned short acceleration_steps_per_s2;  // acceleration steps/sec^2

  float nominal_speed;  // mm/s
  unsigned short nominal_rate;  // steps/s
  
  unsigned short entry_speed;  // mm/s
  unsigned short entry_speed_max;  // mm/s
  unsigned short exit_speed;  // mm/s
  
  char nominal_length_flag;
  char recalculate_flag;
  char busy;
} Segment;


//------------------------------------------------------------------------------
// GLOBALS
//------------------------------------------------------------------------------

extern Segment line_segments[MAX_SEGMENTS];
extern Segment *working_seg;
extern volatile int current_segment;
extern volatile int last_segment;
extern Motor motors[NUM_MOTORS+NUM_SERVOS];
extern const char *AxisNames;
extern const char *MotorNames;
extern float maxFeedRate[NUM_MOTORS];

extern void motor_set_step_count(long *a);
extern void wait_for_empty_segment_buffer();
extern char segment_buffer_full();
extern void motor_line(const float * const target_position,float &fr_mm_s);
extern void motor_engage();
extern void motor_disengage();
extern void motor_setup();
extern void setPenAngle(int arg0);

#endif // MOTOR_H