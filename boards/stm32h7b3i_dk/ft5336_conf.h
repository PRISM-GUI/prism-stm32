/**
  * @file    ft5336_conf.h
  * @brief   FT5336 touch controller configuration: the RK043FN48H panel on the STM32H7B3I-DK
  *          is 480x272, and the controller reports in those axes (BSP_TS_Init is called with
  *          Orientation = TS_SWAP_NONE — if a real board or Renode shows the axes swapped or
  *          mirrored, flip the TS_SWAP_* bits there, and `isRotated` in the Renode platform).
  */
#ifndef FT5336_CONF_H
#define FT5336_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#define FT5336_AUTO_CALIBRATION_ENABLED      0U
#define FT5336_MAX_X_LENGTH                  480U
#define FT5336_MAX_Y_LENGTH                  272U

#ifdef __cplusplus
}
#endif
#endif /* FT5336_CONF_H */
