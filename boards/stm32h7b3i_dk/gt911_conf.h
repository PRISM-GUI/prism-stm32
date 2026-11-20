/**
  * @file    gt911_conf.h
  * @brief   GT911 touch controller configuration. The H7B3I-DK's touch is the FT5336, but the
  *          BSP probes the GT911 first (I2C4 0xBA); its geometry is set to the same panel.
  */
#ifndef GT911_CONF_H
#define GT911_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#define GT911_AUTO_CALIBRATION_ENABLED      0U
#define GT911_MAX_X_LENGTH                  480U
#define GT911_MAX_Y_LENGTH                  272U

#ifdef __cplusplus
}
#endif
#endif /* GT911_CONF_H */
