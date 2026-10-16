#ifndef ACTUATORS_H
#define ACTUATORS_H

void setRelay(bool on);
void setFan(bool on);
void setBlueLed(bool on);
void setBreatheLed(bool on);
void setLedPwm(uint8_t value);
void setRgb(uint8_t r, uint8_t g, uint8_t b);
void setRgbPlant();
void setRgbAuto(bool on);
void setPumpAuto(bool on);
void setFanAuto(bool on);
void setLightAuto(bool on);
void setBreatheAuto(bool on);

#endif
