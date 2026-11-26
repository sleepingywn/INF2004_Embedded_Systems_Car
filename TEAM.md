# SE08 robocar wiring

Pin assignments for the final analogue (A0) firmware on the Robo Pico carrier.
GP numbers refer to Raspberry Pi Pico W GPIOs.

| Pico W GPIO | Connection | Function | Owner |
|---|---|---|---|
| GP2 / GP3 | Right wheel encoder A / B | Quadrature feedback | Buddy 2 |
| GP4 / GP5 | Left wheel encoder A / B | Quadrature feedback | Buddy 2 |
| GP8 / GP9 | Left motor M1 inputs A / B | 10 kHz PWM, slice 4 | Buddy 2 |
| GP10 / GP11 | Right motor M2 inputs A / B | 10 kHz PWM, slice 5 | Buddy 2 |
| GP16 / GP17 | GY-511 IMU SDA / SCL | I²C0 on Grove 4 | Buddy 4 |
| GP19 | External LED with 330 Ω series resistor to GND | Heartbeat indicator | Buddy 1 |
| GP20 | Onboard pushbutton | Active-low run control | Buddy 2 |
| GP22 | Onboard piezo buzzer | Successful barcode decode tone, PWM slice 3A | Buddy 2 |
| GP26 | Barcode sensor A0 | ADC0 | Buddy 3 |
| GP27 | Left line sensor A0 | ADC1 | Buddy 3 |
| GP28 | Right line sensor A0 | ADC2 | Buddy 3 |

Keep the Robo Pico **VBAT-sense jumper open** when connecting the right line
sensor to GP28. The Pico W radio uses GP23, GP24, GP25 and GP29. The onboard
green status LED uses the radio’s `WL_GPIO0`.

Firmware references: [motors, encoders and run button](app_program/subsystem2/motion_config.h),
[buzzer](app_program/subsystem2/drv_motor.c), [IR channels](app_program/subsystem3/ir_types.h),
[IMU bus pins](device/i2c/sysdepend/rp2040/i2c_cnf_sysdep.h) and
[heartbeat LED](app_program/subsystem1/subsystem1.h).
