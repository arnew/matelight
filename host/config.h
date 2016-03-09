#ifndef __CONFIG_H__
#define __CONFIG_H__

#define DISPLAY_WIDTH		30	
#define DISPLAY_HEIGHT		12	
#define FRAME_TIMEOUT		5000 /* ms */
#define UDP_PORT			2323
#define FRAMERATE			20 /* fps */
#define TEXT_LOOP_COUNT		3
#define TEXT_SCROLL_SPEED	3

#define MATELIGHT_VID					0x1cbe
#define MATELIGHT_PID					0x0003

#define CRATE_WIDTH						6
#define CRATE_HEIGHT					4

#define MATELIGHT_FRAMEDATA_ENDPOINT	0x01
#define MATELIGHT_TIMEOUT				1000

#define GAMMA							2.5F
#define C0 {0,0,0}
#define CC_1W {\
{C0,C0,C0,C0,C0},\
{C0,C0,C0,C0,C0},\
{C0,C0,C0,C0,C0}}
#define CC_p5W {\
{C0,C0,C0,C0,C0},\
{C0,C0,C0,C0,C0},\
{C0,C0,C0,C0,C0}}
#define CC_dither 30.0F

#define gerade(y2,y1,x2,x1,x) (((y2)-(y1))/((x2)-(x1))*((x)-(x1))+(y1))
#define cc_APPLY(c,i) ((uint8_t)roundf(  gerade(cc_1w[cy][cx][i],cc_p5w[cy][cx][i],255 + CC_dither, 128+CC_dither/2, c + rand()*(CC_dither/(1.0F*RAND_MAX))   )))
//#define cc_APPLY(c,i) ((uint8_t)roundf(((c+(rand()*(CC_dither*1.0F/RAND_MAX)))/(255.0F+CC_dither)) * cc[cy][cx][i] * 255))

#endif//__CONFIG_H__
