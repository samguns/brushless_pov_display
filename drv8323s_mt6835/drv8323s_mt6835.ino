/*
    Odrive robotics' hardware is one of the best  BLDC motor foc supporting hardware out there.

    This is an example code that can be directly uploaded to the Odrive using the SWD programmer. 
    This code uses an magnetic spi sensor AS5047 and a BLDC motor with 11 pole pairs connected to the M0 interface of the Odrive. 

    This is a short template code and the idea is that you are able to adapt to your needs not to be a complete solution. :D 
*/
#include <SimpleFOC.h>
#include <SimpleFOCDrivers.h>
#include "drivers/drv8323/drv8323.h"
#include <encoders/mt6835/MagneticSensorMT6835.h>

// Odrive M0 motor pinout
#define M0_INH_A PA10
#define M0_INH_B PA9
#define M0_INH_C PA8
#define M0_INL_A PB15
#define M0_INL_B PB14
#define M0_INL_C PB13
// M0 currnets
#define M0_IA PA0
#define M0_IB PA1
// Odrive M0 encoder pinout
#define M0_ENC_A PA4

// M1 & M2 common enable pin
#define EN_GATE PB12

#define IOUTA PA2
#define IOUTB PA1
#define IOUTC PA0

// SPI pinout
#define SPI3_CS PC13
#define SPI3_SCL PC10
#define SPI3_MISO PC11
#define SPI3_MOSI PC12

// Motor instance
BLDCMotor motor = BLDCMotor(7);
// BLDCDriver6PWM driver = BLDCDriver6PWM(M0_INH_A,M0_INL_A, M0_INH_B,M0_INL_B, M0_INH_C,M0_INL_C, EN_GATE);
DRV8323Driver6PWM driver = DRV8323Driver6PWM(M0_INH_A, M0_INL_A, M0_INH_B, M0_INL_B, M0_INH_C, M0_INL_C, SPI3_CS, EN_GATE);

// default velocity-mode target [rad/s]
#define DEFAULT_TARGET_VELOCITY 70.0f

// instantiate the commander
Commander command = Commander(Serial);
void doMotor(char* cmd) {
  command.motor(&motor, cmd);
}

// low side current sensing define
// 0.0005 Ohm resistor
// gain of 10x
// current sensing on B and C phases, phase A not connected
// LowsideCurrentSense current_sense = LowsideCurrentSense(0.005f, 12.22f, IOUTA, IOUTB, IOUTC);
LowsideCurrentSense current_sense = LowsideCurrentSense(0.001f, 40.0f, IOUTA, IOUTB, _NC);

// MagneticSensorSPI(int cs, float _cpr, int _angle_register)
// config           - SPI config
//  cs              - SPI chip select pin
// MagneticSensorSPI sensor = MagneticSensorSPI(AS5147_SPI, M0_ENC_A);
MagneticSensorMT6835 sensor = MagneticSensorMT6835(M0_ENC_A);
SPIClass SPI_3(SPI3_MOSI, SPI3_MISO, SPI3_SCL);

void read_driver_status() {
  Fault_Status_1 fault_status;
  fault_status.reg = driver.readSPI(Fault_Status_1_ADDR);
  Serial.print("DRV8323 Fault Status: ");
  Serial.println(fault_status.reg);
  delay(1);

  Driver_Control ctrl;
  ctrl.reg = driver.readSPI(Driver_Control_ADDR);
  Serial.print("DRV8323 Driver Control: ");
  Serial.println(ctrl.reg);
  delay(1);

  Gate_Drive_HS gate_drive_hs_ctrl;
  gate_drive_hs_ctrl.reg = driver.readSPI(Gate_Drive_HS_ADDR);
  Serial.print("DRV8323 Gate Driver HS Control: ");
  Serial.println(gate_drive_hs_ctrl.reg);
  delay(1);

  Gate_Drive_LS gate_drive_ls_ctrl;
  gate_drive_ls_ctrl.reg = driver.readSPI(Gate_Drive_LS_ADDR);
  Serial.print("DRV8323 Gate Driver LS Control: ");
  Serial.println(gate_drive_ls_ctrl.reg);
  delay(1);

  OCP_Control ocp_ctrl;
  ocp_ctrl.reg = driver.readSPI(OCP_Control_ADDR);
  Serial.print("DRV8323 OCP Control: ");
  Serial.println(ocp_ctrl.reg);
  delay(1);

  CSA_Control csa_ctrl;
  csa_ctrl.reg = driver.readSPI(CSA_Control_ADDR);
  Serial.print("DRV8323 CSA Control: ");
  Serial.println(csa_ctrl.reg);
}

void setup() {

  // use monitoring with serial
  Serial.begin(115200);

  delay(1000);

  Serial.println("Sensor ready");
  // enable more verbose output for debugging
  // comment out if not needed
  SimpleFOCDebug::enable(&Serial);

  // pwm frequency to be used [Hz]
  driver.pwm_frequency = 25000;
  // power supply voltage [V]
  driver.voltage_power_supply = 12;
  // Max DC voltage allowed - default voltage_power_supply
  driver.voltage_limit = 12;
  // driver init
  driver.init(&SPI_3);
  driver.setGateDriveHS(60.0, 60.0);
  delay(1);
  driver.setGateDriveLS(60.0, 60.0, 500);
  delay(1);
  driver.setOCP(0.26, 50.0, AutoRetry_Fault, 4.0);
  delay(10);
  // link the motor and the driver
  motor.linkDriver(&driver);

  // initialise magnetic sensor hardware
  // sensor.init(&SPI_3);
  sensor.init();
  // link the motor to the sensor
  motor.linkSensor(&sensor);

  // control loop type and torque mode
  motor.foc_modulation = FOCModulationType::SpaceVectorPWM;
  motor.torque_controller = TorqueControlType::foc_current;
  motor.controller = MotionControlType::velocity;

  // controller configuration based on the control type
  motor.PID_velocity.P = 0.3f;
  motor.PID_velocity.I = 20.0f;
  motor.PID_velocity.D = 0;

  // velocity low pass filtering time constant
  motor.LPF_velocity.Tf = 0.01f;

  // current q loop PID
  motor.PID_current_q.P = 1.0;
  motor.PID_current_q.I = 100;
  // current d loop PID
  motor.PID_current_d.P = 1.0;
  motor.PID_current_d.I = 100;

  // max voltage  allowed for motion control
  motor.voltage_limit = 12.0;
  motor.velocity_limit = 300;
  motor.current_limit = 6.0f;
  // alignment voltage limit
  motor.voltage_sensor_align = 1;

  // comment out if not needed
  motor.useMonitoring(Serial);
  motor.monitor_variables = _MON_CURR_Q | _MON_CURR_D | _MON_TARGET | _MON_VEL | _MON_ANGLE;
  motor.monitor_downsample = 0;

  // add target command T
  command.add('M', doMotor, "motor M0");

  // initialise motor
  motor.init();

  // delay(1000);
  driver.setCSA(Gain_40V, true);
  delay(1500);
  driver.setCSA(Gain_40V, false);
  // delay(500);

  read_driver_status();
  delay(10);

  // link the driver
  current_sense.linkDriver(&driver);
  // init the current sense
  current_sense.init();
  // current_sense.gain_a *= -1;
  // current_sense.gain_b *= -1;
  // current_sense.gain_c *= -1;
  current_sense.skip_align = false;
  motor.linkCurrentSense(&current_sense);

  // init FOC
  motor.initFOC();

  // default running setpoint: velocity control at DEFAULT_TARGET_VELOCITY rad/s
  motor.target = DEFAULT_TARGET_VELOCITY;
  Serial.print("Default velocity target [rad/s]: ");
  Serial.println(motor.target);
}

void loop() {

  // foc loop
  motor.loopFOC();
  // motion control
  motor.move();
  // monitoring
  motor.monitor();
  // user communication
  command.run();
}