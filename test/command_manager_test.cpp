#include <gtest/gtest.h>
#include "rosflight.h"
#include "test_board.h"

#define EXPECT_INTHESAMEBALLPARK(x, y) EXPECT_LE(fabs(x - y), 0.1)
#define EXPECT_PRETTYCLOSE(x, y) EXPECT_LE(fabs(x - y), 0.01)
#define EXPECT_CLOSE(x, y) EXPECT_LE(fabs(x - y), 0.001)
#define EXPECT_BASICALLYTHESAME(x, y) EXPECT_LE(fabs(x - y), 0.00001)


using namespace rosflight_firmware;

// Initialize the full firmware, so that the state_manager can do its thing
void step_firmware(ROSflight& rf, testBoard& board, uint32_t us)
{
  board.set_time(board.clock_micros() + us);
  rf.rosflight_run();
}

TEST(command_manager_test, rc) {
  testBoard board;
  ROSflight rf(board);

  // Initialize the firmware
  rf.rosflight_init();

  uint16_t rc_values[8];
  for (int i = 0; i < 8; i++)
  {
    rc_values[i] = 1500;
  }
  rc_values[2] = 1000;

  float max_roll = rf.params_.get_param_float(PARAM_RC_MAX_ROLL);
  float max_pitch = rf.params_.get_param_float(PARAM_RC_MAX_PITCH);
  float max_yawrate = rf.params_.get_param_float(PARAM_RC_MAX_YAWRATE);


  //=================================================
  // RC Commands Test
  //=================================================
  // First, lets just try sending rc_commands alone
  board.set_rc(rc_values);
  step_firmware(rf, board, 20000);

  control_t output = rf.command_manager_.combined_control();
  EXPECT_EQ(output.x.type, ANGLE);
  EXPECT_PRETTYCLOSE(output.x.value, 0.0);
  EXPECT_EQ(output.y.type, ANGLE);
  EXPECT_PRETTYCLOSE(output.y.value, 0.0);
  EXPECT_EQ(output.z.type, RATE);
  EXPECT_PRETTYCLOSE(output.z.value, 0.0);
  EXPECT_EQ(output.F.type, THROTTLE);
  EXPECT_PRETTYCLOSE(output.F.value, 0.0);

  rc_values[0] = 2000;
  rc_values[1] = 1000;
  rc_values[2] = 1500;
  rc_values[3] = 1250;
  board.set_rc(rc_values);
  step_firmware(rf, board, 20000);

  output = rf.command_manager_.combined_control();
  EXPECT_EQ(output.x.type, ANGLE);
  EXPECT_PRETTYCLOSE(output.x.value, 1.0*max_roll);
  EXPECT_EQ(output.y.type, ANGLE);
  EXPECT_PRETTYCLOSE(output.y.value, -1.0*max_pitch);
  EXPECT_EQ(output.z.type, RATE);
  EXPECT_PRETTYCLOSE(output.z.value, -0.5*max_yawrate);
  EXPECT_EQ(output.F.type, THROTTLE);
  EXPECT_PRETTYCLOSE(output.F.value, 0.5);
}


TEST(command_manager_test, rc_arm_disarm) {
  testBoard board;
  ROSflight rf(board);

  // Make sure that rc is hooked up
  board.set_pwm_lost(false);

  // Initialize the firmware
  rf.rosflight_init();

  uint16_t rc_values[8];
  for (int i = 0; i < 8; i++)
  {
    rc_values[i] = 1500;
  }
  rc_values[2] = 1000;

  float max_roll = rf.params_.get_param_float(PARAM_RC_MAX_ROLL);
  float max_pitch = rf.params_.get_param_float(PARAM_RC_MAX_PITCH);
  float max_yawrate = rf.params_.get_param_float(PARAM_RC_MAX_YAWRATE);

  // Let's clear all errors in the state_manager
  rf.state_manager_.clear_error(rf.state_manager_.state().error_codes);

  //=================================================
  // RC Arming Test
  //=================================================

  // Let's send an arming signal
  rc_values[0] = 1500;
  rc_values[1] = 1500;
  rc_values[2] = 1000;
  rc_values[3] = 2000;
  board.set_rc(rc_values);
//  step_firmware(rf, board, 20000);
  // Step long enough for an arm to happen
  while (board.clock_millis() < 1200000)
  {
    step_firmware(rf, board, 20000);
  }
  // Check the output
  control_t output = rf.command_manager_.combined_control();
  EXPECT_EQ(output.x.type, ANGLE);
  EXPECT_PRETTYCLOSE(output.x.value, 0.0*max_roll);
  EXPECT_EQ(output.y.type, ANGLE);
  EXPECT_PRETTYCLOSE(output.y.value, 0.0*max_pitch);
  EXPECT_EQ(output.z.type, RATE);
  EXPECT_PRETTYCLOSE(output.z.value, 1.0*max_yawrate);
  EXPECT_EQ(output.F.type, THROTTLE);
  EXPECT_PRETTYCLOSE(output.F.value, 0.0);

  // See that we are armed
  EXPECT_EQ(rf.state_manager_.state().armed, true);

  // Let's send a disarming signal
  rc_values[0] = 1500;
  rc_values[1] = 1500;
  rc_values[2] = 1000;
  rc_values[3] = 1000;
  board.set_rc(rc_values);
  // Step long enough for an arm to happen
  step_firmware(rf, board, 1200000);

  // See that we are disarmed
  EXPECT_EQ(rf.state_manager_.state().armed, false);
  EXPECT_EQ(rf.state_manager_.state().error, false);
  EXPECT_EQ(rf.state_manager_.state().failsafe, false);


  //=================================================
  // Switch Arming Test
  //=================================================
  rf.params_.set_param_int(PARAM_RC_ARM_CHANNEL, 4);
  rc_values[0] = 1500;
  rc_values[1] = 1500;
  rc_values[2] = 1000;
  rc_values[3] = 1500;
  rc_values[4] = 1500;

  // Set all stick neutral position
  board.set_rc(rc_values);
  step_firmware(rf, board, 20000);
  // make sure we are still disarmed
  EXPECT_EQ(rf.state_manager_.state().armed, false);
  EXPECT_EQ(rf.state_manager_.state().error, false);
  EXPECT_EQ(rf.state_manager_.state().failsafe, false);

  // flip the arm switch on
  rc_values[4] = 1900;
  board.set_rc(rc_values);
  step_firmware(rf, board, 20000);
  // we should be armed
  EXPECT_EQ(rf.state_manager_.state().armed, true);
  EXPECT_EQ(rf.state_manager_.state().error, false);
  EXPECT_EQ(rf.state_manager_.state().failsafe, false);

  // flip the arm switch off
  rc_values[4] = 1100;
  board.set_rc(rc_values);
  step_firmware(rf, board, 20000);
  // we should be disarmed
  EXPECT_EQ(rf.state_manager_.state().armed, false);
  EXPECT_EQ(rf.state_manager_.state().error, false);
  EXPECT_EQ(rf.state_manager_.state().failsafe, false);
}


TEST(command_manager_test, rc_failsafe_test) {
  testBoard board;
  ROSflight rf(board);

  // Make sure that rc is hooked up
  board.set_pwm_lost(false);

  // Initialize the firmware
  rf.rosflight_init();

  uint16_t rc_values[8];
  for (int i = 0; i < 8; i++)
  {
    rc_values[i] = 1500;
  }
  rc_values[2] = 1000;

  float max_roll = rf.params_.get_param_float(PARAM_RC_MAX_ROLL);
  float max_pitch = rf.params_.get_param_float(PARAM_RC_MAX_PITCH);
  float max_yawrate = rf.params_.get_param_float(PARAM_RC_MAX_YAWRATE);
  float failsafe_throttle = rf.params_.get_param_float(PARAM_FAILSAFE_THROTTLE);

  // Let's clear all errors in the state_manager
  rf.state_manager_.clear_error(rf.state_manager_.state().error_codes);

  //=================================================
  // Disarmed Failsafe
  //=================================================

  // Let's go into failsafe while disarmed
  board.set_pwm_lost(true);
  step_firmware(rf, board, 20000);
  // Check the output - This should be the failsafe control
  control_t output = rf.command_manager_.combined_control();
  EXPECT_EQ(output.x.type, ANGLE);
  EXPECT_PRETTYCLOSE(output.x.value, 0.0*max_roll);
  EXPECT_EQ(output.y.type, ANGLE);
  EXPECT_PRETTYCLOSE(output.y.value, 0.0*max_pitch);
  EXPECT_EQ(output.z.type, RATE);
  EXPECT_PRETTYCLOSE(output.z.value, 0.0*max_yawrate);
  EXPECT_EQ(output.F.type, THROTTLE);
  EXPECT_PRETTYCLOSE(output.F.value, failsafe_throttle);

  // We should also be disarmed, in failsafe and in error
  EXPECT_EQ(rf.state_manager_.state().armed, false);
  EXPECT_EQ(rf.state_manager_.state().failsafe, true);
  EXPECT_EQ(rf.state_manager_.state().error, true);
  EXPECT_EQ(rf.state_manager_.state().error_codes, StateManager::ERROR_RC_LOST);

  // Lets clear the failsafe
  board.set_pwm_lost(false);
  board.set_rc(rc_values);
  step_firmware(rf, board, 20000);
  output = rf.command_manager_.combined_control();
  EXPECT_PRETTYCLOSE(output.x.value, 0.0*max_roll);
  EXPECT_PRETTYCLOSE(output.y.value, 0.0*max_pitch);
  EXPECT_PRETTYCLOSE(output.z.value, 0.0*max_yawrate);
  EXPECT_PRETTYCLOSE(output.F.value, 0.0);

  // We should still be disarmed, but no failsafe or error
  EXPECT_EQ(rf.state_manager_.state().armed, false);
  EXPECT_EQ(rf.state_manager_.state().failsafe, false);
  EXPECT_EQ(rf.state_manager_.state().error, false);
  EXPECT_EQ(rf.state_manager_.state().error_codes, 0x00);

  //=================================================
  // Armed Failsafe
  //=================================================

  // Let's send an arming signal
  rc_values[0] = 1500;
  rc_values[1] = 1500;
  rc_values[2] = 1000;
  rc_values[3] = 2000;
  board.set_rc(rc_values);
  // Step long enough for an arm to happen
  while (board.clock_millis() < 1200000)
  {
    step_firmware(rf, board, 20000);
  }

  // check that we are armed
  EXPECT_EQ(rf.state_manager_.state().armed, true);
  EXPECT_EQ(rf.state_manager_.state().error, false);
  EXPECT_EQ(rf.state_manager_.state().failsafe, false);

  // Set a command on the sticks
  rc_values[0] = 1750;
  rc_values[1] = 1250;
  rc_values[2] = 1600;
  rc_values[3] = 1100;

  // Lost RC
  board.set_rc(rc_values);
  board.set_pwm_lost(true);
  step_firmware(rf, board, 20000);
  // Check the output - This should be the failsafe control
  output = rf.command_manager_.combined_control();
  EXPECT_EQ(output.x.type, ANGLE);
  EXPECT_PRETTYCLOSE(output.x.value, 0.0*max_roll);
  EXPECT_EQ(output.y.type, ANGLE);
  EXPECT_PRETTYCLOSE(output.y.value, 0.0*max_pitch);
  EXPECT_EQ(output.z.type, RATE);
  EXPECT_PRETTYCLOSE(output.z.value, 0.0*max_yawrate);
  EXPECT_EQ(output.F.type, THROTTLE);
  EXPECT_PRETTYCLOSE(output.F.value, failsafe_throttle);

  // We should still be armed, but now in failsafe
  EXPECT_EQ(rf.state_manager_.state().armed, true);
  EXPECT_EQ(rf.state_manager_.state().error, true);
  EXPECT_EQ(rf.state_manager_.state().failsafe, true);

  // Regain RC
  board.set_pwm_lost(false);
  step_firmware(rf, board, 20000);
  // Check the output - This should be our rc control
  output = rf.command_manager_.combined_control();
  EXPECT_EQ(output.x.type, ANGLE);
  EXPECT_PRETTYCLOSE(output.x.value, 0.5*max_roll);
  EXPECT_EQ(output.y.type, ANGLE);
  EXPECT_PRETTYCLOSE(output.y.value, -0.5*max_pitch);
  EXPECT_EQ(output.z.type, RATE);
  EXPECT_PRETTYCLOSE(output.z.value, -0.8*max_yawrate);
  EXPECT_EQ(output.F.type, THROTTLE);
  EXPECT_PRETTYCLOSE(output.F.value, 0.6);
}


TEST(command_manager_test, rc_offboard_muxing_test ) {

  testBoard board;
  ROSflight rf(board);

  // Make sure that rc is hooked up
  board.set_pwm_lost(false);

  // Initialize the firmware
  rf.rosflight_init();

  uint16_t rc_values[8];
  for (int i = 0; i < 8; i++)
  {
    rc_values[i] = 1500;
  }
  rc_values[2] = 1000;

  float max_roll = rf.params_.get_param_float(PARAM_RC_MAX_ROLL);

  // Let's clear all errors in the state_manager
  rf.state_manager_.clear_error(rf.state_manager_.state().error_codes);

  //=================================================
  // Offboard Command Integration
  //=================================================

  control_t offboard_command =
  {
    20000,
    {true, ANGLE, -1.0},
    {true, ANGLE, 0.5},
    {true, RATE, -0.7},
    {true, THROTTLE, 0.9}
  };

  // First, just set an offboard command and rc, mux it and see what happens
  board.set_rc(rc_values);
  // step a bunch of times to clear the "lag time" on RC
  while (board.clock_micros() < 1000000)
  {
    step_firmware(rf, board, 20000);
  }

  rf.command_manager_.set_new_offboard_command(offboard_command);
  step_firmware(rf, board, 20000);

  // We don't have an override channel mapped, so this should be RC
  control_t output = rf.command_manager_.combined_control();
  EXPECT_PRETTYCLOSE(output.x.value, 0.0);
  EXPECT_PRETTYCLOSE(output.y.value, 0.0);
  EXPECT_PRETTYCLOSE(output.z.value, 0.0);
  EXPECT_PRETTYCLOSE(output.F.value, 0.0);

  rf.params_.set_param_int(PARAM_RC_ATTITUDE_OVERRIDE_CHANNEL, 4);
  rf.params_.set_param_int(PARAM_RC_THROTTLE_OVERRIDE_CHANNEL, 4);

  // ensure that the override switch is off
  rc_values[4] = 1100;

  board.set_rc(rc_values);
  offboard_command.stamp_us = board.clock_micros();
  rf.command_manager_.set_new_offboard_command(offboard_command);
  step_firmware(rf, board, 20000);

  // This should be offboard
  output = rf.command_manager_.combined_control();
  EXPECT_PRETTYCLOSE(output.x.value, -1.0);
  EXPECT_PRETTYCLOSE(output.y.value, 0.5);
  EXPECT_PRETTYCLOSE(output.z.value, -0.7);
  EXPECT_PRETTYCLOSE(output.F.value, 0.9);

  // flip override switch on
  rc_values[4] = 1900;
  board.set_rc(rc_values);
  offboard_command.stamp_us = board.clock_micros();
  rf.command_manager_.set_new_offboard_command(offboard_command);
  step_firmware(rf, board, 20000);

  // This should be RC
  output = rf.command_manager_.combined_control();
  EXPECT_PRETTYCLOSE(output.x.value, 0.0);
  EXPECT_PRETTYCLOSE(output.y.value, 0.0);
  EXPECT_PRETTYCLOSE(output.z.value, 0.0);
  EXPECT_PRETTYCLOSE(output.F.value, 0.0);


  //=================================================
  // Partial Offboard Command Integration
  //=================================================

  // Only override attitude
  rf.params_.set_param_int(PARAM_RC_ATTITUDE_OVERRIDE_CHANNEL, 4);
  rf.params_.set_param_int(PARAM_RC_THROTTLE_OVERRIDE_CHANNEL, -1);

  board.set_rc(rc_values);
  offboard_command.stamp_us = board.clock_micros();
  rf.command_manager_.set_new_offboard_command(offboard_command);
  step_firmware(rf, board, 20000);

  // Throttle should be offboard
  output = rf.command_manager_.combined_control();
  EXPECT_PRETTYCLOSE(output.x.value, 0.0);
  EXPECT_PRETTYCLOSE(output.y.value, 0.0);
  EXPECT_PRETTYCLOSE(output.z.value, 0.0);
  EXPECT_PRETTYCLOSE(output.F.value, 0.9);


  // Only override throttle
  rf.params_.set_param_int(PARAM_RC_ATTITUDE_OVERRIDE_CHANNEL, -1);
  rf.params_.set_param_int(PARAM_RC_THROTTLE_OVERRIDE_CHANNEL, 4);

  board.set_rc(rc_values);
  offboard_command.stamp_us = board.clock_micros();
  rf.command_manager_.set_new_offboard_command(offboard_command);
  step_firmware(rf, board, 20000);

  // Throttle should be rc
  output = rf.command_manager_.combined_control();
  EXPECT_PRETTYCLOSE(output.x.value, -1.0);
  EXPECT_PRETTYCLOSE(output.y.value, 0.5);
  EXPECT_PRETTYCLOSE(output.z.value, -0.7);
  EXPECT_PRETTYCLOSE(output.F.value, 0.0);


  //=================================================
  // RC Intervention
  //=================================================

  // Only override attitude
  rf.params_.set_param_int(PARAM_RC_ATTITUDE_OVERRIDE_CHANNEL, 4);
  rf.params_.set_param_int(PARAM_RC_THROTTLE_OVERRIDE_CHANNEL, 4);

  // switch is off, but roll channel is deviated
  rc_values[4] = 1100;
  rc_values[0] = 1800;
  rc_values[2] = 1000;

  board.set_rc(rc_values);
  offboard_command.stamp_us = board.clock_micros();
  rf.command_manager_.set_new_offboard_command(offboard_command);
  step_firmware(rf, board, 20000);

  output = rf.command_manager_.combined_control();
  EXPECT_PRETTYCLOSE(output.x.value, 0.6*max_roll);  // This channel should be overwritten
  EXPECT_PRETTYCLOSE(output.y.value, 0.5);
  EXPECT_PRETTYCLOSE(output.z.value, -0.7);
  EXPECT_PRETTYCLOSE(output.F.value, 0.9);

  //=================================================
  // RC Override Lag Test
  //=================================================

  // Switch is still off, and rc no longer deviated.  There should still be lag though
  rc_values[0] = 1500;
  rc_values[2] = 1000;
  board.set_rc(rc_values);
  uint64_t start_ms = board.clock_millis();
  step_firmware(rf, board, 20000);
  while (board.clock_millis() < rf.params_.get_param_int(PARAM_OVERRIDE_LAG_TIME) + start_ms)
  {
    output = rf.command_manager_.combined_control();
    EXPECT_PRETTYCLOSE(output.x.value, 0.0);  // This channel should be overwritten
    EXPECT_PRETTYCLOSE(output.y.value, 0.5);
    EXPECT_PRETTYCLOSE(output.z.value, -0.7);
    EXPECT_PRETTYCLOSE(output.F.value, 0.9);
    offboard_command.stamp_us = board.clock_micros();
    rf.command_manager_.set_new_offboard_command(offboard_command);
    step_firmware(rf, board, 20000);
  }
  offboard_command.stamp_us = board.clock_micros();
  rf.command_manager_.set_new_offboard_command(offboard_command);
  step_firmware(rf, board, 20000);
  output = rf.command_manager_.combined_control();
  EXPECT_PRETTYCLOSE(output.x.value, -1.0);  // This channel should no longer be overwritten


  //=================================================
  // Offboard Command Stale
  //=================================================

  start_ms = board.clock_millis();
  while (board.clock_millis() < 100 + start_ms)
  {
    output = rf.command_manager_.combined_control();
    EXPECT_PRETTYCLOSE(output.x.value, -1.0);  // Offboard Command is still valid
    EXPECT_PRETTYCLOSE(output.y.value, 0.5);
    EXPECT_PRETTYCLOSE(output.z.value, -0.7);
    EXPECT_PRETTYCLOSE(output.F.value, 0.9);
    step_firmware(rf, board, 20000);
  }

  // Offboard command has timed out -> revert to RC
  output = rf.command_manager_.combined_control();
  EXPECT_PRETTYCLOSE(output.x.value, 0.0);
  EXPECT_PRETTYCLOSE(output.y.value, 0.0);
  EXPECT_PRETTYCLOSE(output.z.value, 0.0);
  EXPECT_PRETTYCLOSE(output.F.value, 0.0);
}


TEST(command_manager_test, partial_muxing_test ) {

  testBoard board;
  ROSflight rf(board);

  // Make sure that rc is hooked up
  board.set_pwm_lost(false);

  // Initialize the firmware
  rf.rosflight_init();

  uint16_t rc_values[8];
  for (int i = 0; i < 8; i++)
  {
    rc_values[i] = 1500;
  }
  rc_values[2] = 1000;

  float max_roll = rf.params_.get_param_float(PARAM_RC_MAX_ROLL);

  // Let's clear all errors in the state_manager
  rf.state_manager_.clear_error(rf.state_manager_.state().error_codes);

  //=================================================
  // Min Throttle Test
  //=================================================

  rf.params_.set_param_int(PARAM_RC_OVERRIDE_TAKE_MIN_THROTTLE, true);

  control_t offboard_command =
  {
    20000,
    {true, ANGLE, -1.0},
    {true, ANGLE, 0.5},
    {true, RATE, -0.7},
    {true, THROTTLE, 0.5}
  };

  // step a bunch of times to clear the "lag time" on RC
  while (board.clock_micros() < 1000000)
  {
    step_firmware(rf, board, 20000);
  }

  rc_values[2] = 1200;

  // RC is the min throttle
  board.set_rc(rc_values);
  offboard_command.stamp_us = board.clock_micros();
  rf.command_manager_.set_new_offboard_command(offboard_command);
  step_firmware(rf, board, 20000);
  control_t output = rf.command_manager_.combined_control();
  EXPECT_PRETTYCLOSE(output.F.value, 0.2);

  // Now, offboard is the min throttle
  offboard_command.F.value = 0.2;
  rc_values[2] = 1500;

  board.set_rc(rc_values);
  offboard_command.stamp_us = board.clock_micros();
  rf.command_manager_.set_new_offboard_command(offboard_command);
  step_firmware(rf, board, 20000);
  output = rf.command_manager_.combined_control();
  EXPECT_PRETTYCLOSE(output.F.value, 0.2);



  // Okay, remove the Min throttle setting
  rf.params_.set_param_int(PARAM_RC_OVERRIDE_TAKE_MIN_THROTTLE, false);


  // Now, let's disable the pitch channel on the onboard command
  offboard_command.y.active = false;
  offboard_command.stamp_us = board.clock_micros();
  rf.command_manager_.set_new_offboard_command(offboard_command);

  step_firmware(rf, board, 20000);
  output = rf.command_manager_.combined_control();
  EXPECT_PRETTYCLOSE(output.x.value, -1.0);
  EXPECT_PRETTYCLOSE(output.y.value, 0.0);
  EXPECT_PRETTYCLOSE(output.z.value, -0.7);
  EXPECT_PRETTYCLOSE(output.F.value, 0.2);

  // Let's change the type on the x channel
  offboard_command.x.type = RATE;
  offboard_command.stamp_us = board.clock_micros();
  rf.command_manager_.set_new_offboard_command(offboard_command);

  step_firmware(rf, board, 20000);
  output = rf.command_manager_.combined_control();
  EXPECT_EQ(output.x.type, RATE);
  EXPECT_EQ(output.y.type, ANGLE);
  EXPECT_EQ(output.z.type, RATE);
  EXPECT_EQ(output.F.type, THROTTLE);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
