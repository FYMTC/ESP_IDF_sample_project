#include "hal_lvgl.hpp"
#include "conf.h"
#include "bt.hpp"
static const char *TAG = "lvgl_port";
/* Display */
LGFX_tft tft;
static void disp_init(void);
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
/* Touch pad */
CTP ctp;

mouse_t bt_mouse_indev;
uint8_t notifyCallback_statue=0;

static void touchpad_init(void);
static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
lv_indev_t *indev_touchpad;
lv_indev_t *indev_mouse;
lv_indev_t *indev_keypad;
lv_indev_t *indev_encoder;
lv_indev_t *indev_button;

static bool touchpad_is_pressed(void);
static void touchpad_get_xy(lv_coord_t * x, lv_coord_t * y);

static void mouse_init(void);
static void mouse_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static bool mouse_is_pressed(void);
static void mouse_get_xy(lv_coord_t * x, lv_coord_t * y);

static void keypad_init(void);
static void keypad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static uint32_t keypad_get_key(void);

static void encoder_init(void);
static void encoder_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static void encoder_handler(void);

static void button_init(void);
static void button_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);
static int8_t button_get_pressed_id(void);
static bool button_is_pressed(uint8_t id);
/***********************************************************/
/**********************    lvgl ↓   *********************/
void lv_port_indev_init(void)
{
    /**
     * Here you will find example implementation of input devices supported by LittelvGL:
     *  - Touchpad
     *  - Mouse (with cursor support)
     *  - Keypad (supports GUI usage only with key)
     *  - Encoder (supports GUI usage only with: left, right, push)
     *  - Button (external buttons to press points on the screen)
     *
     *  The `..._read()` function are only examples.
     *  You should shape them according to your hardware
     */

    static lv_indev_drv_t indev_drv;
    /*------------------
     * Touchpad
     * -----------------*/
    #if USE_TOUCHPAD
    /*Initialize your touchpad if you have*/
    touchpad_init();

    /*Register a touchpad input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);
    #endif

    /*------------------
     * Mouse
     * -----------------*/

    /*Initialize your mouse if you have*/
    #if USE_MOUSE
    mouse_init();

    /*Register a mouse input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = mouse_read;
    indev_mouse = lv_indev_drv_register(&indev_drv);

    /*Set cursor. For simplicity set a HOME symbol now.*/
    lv_obj_t * mouse_cursor = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(mouse_cursor, &NotoSansSC_Medium_3500, 0);
    lv_img_set_src(mouse_cursor, "\xEF\x89\x85");//鼠标字体符号
    lv_indev_set_cursor(indev_mouse, mouse_cursor);
    #endif

}

/*------------------
 * 触摸设备
 * -----------------*/

/*Initialize your touchpad*/
static void touchpad_init(void)
{
    /*Your code comes here*/
    ctp.begin(I2C_NUM_1);
}

/*Will be called by the library to read the touchpad*/
static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    // static lv_coord_t last_x = 0;
    // static lv_coord_t last_y = 0;

    static int x_pos = 0;
    static int y_pos = 0;

    /*Save the pressed coordinates and the state*/
    // if(touchpad_is_pressed()) {
    //     touchpad_get_xy(&last_x, &last_y);
    //     data->state = LV_INDEV_STATE_PR;
    // }
    // else {
    //     data->state = LV_INDEV_STATE_REL;
    // }

    if (ctp.is_touched())
    {
        ctp.get_touch_pos(&x_pos, &y_pos);
        // Serial.printf("X:%d Y:%d\n", x_pos, y_pos);
        data->point.x = x_pos;
        data->point.y = y_pos;
        data->state = LV_INDEV_STATE_PR;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }

    /*Set the last pressed coordinates*/
    // data->point.x = last_x;
    // data->point.y = last_y;
}
/*------------------
 * Mouse
 * -----------------*/

/*Initialize your mouse*/
static void mouse_init(void)
{
    /*Your code comes here*/
    bt_mouse_indev.left_button_pressed =false;
    bt_mouse_indev.right_button_pressed =false;
    bt_mouse_indev.x_movement =0;
    bt_mouse_indev.y_movement =0;
    bt_mouse_indev.data_frame=0;
}

/*Will be called by the library to read the mouse*/
static void mouse_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    /*Get the current x and y coordinates*/
    mouse_get_xy(&data->point.x, &data->point.y);

    /*Get whether the mouse button is pressed or released*/
    if(mouse_is_pressed()) {
        data->state = LV_INDEV_STATE_PR;
    }
    else {
        data->state = LV_INDEV_STATE_REL;
    }
}

/*Return true is the mouse button is pressed*/
static bool mouse_is_pressed(void)
{
    /*Your code comes here*/

    return bt_mouse_indev.left_button_pressed;
}

/*Get the x and y coordinates if the mouse is pressed*/
static void mouse_get_xy(lv_coord_t * x, lv_coord_t * y)
{
    /*Your code comes here*/
    static lv_point_t last_pos = {0, 0}; // 保存鼠标的最后位置
    if(notifyCallback_statue !=bt_mouse_indev.data_frame ){//说明鼠标数据有更新
    last_pos.x += bt_mouse_indev.x_movement;
    last_pos.y += bt_mouse_indev.y_movement;
    if(last_pos.y>MY_DISP_VER_RES){last_pos.y=MY_DISP_VER_RES;}
    if(last_pos.x>MY_DISP_HOR_RES){last_pos.x=MY_DISP_HOR_RES;}
    if(last_pos.x<0){last_pos.x=0;}
    if(last_pos.y<0){last_pos.y=0;}
    (*x) = last_pos.x;
    (*y) = last_pos.y;
    notifyCallback_statue =bt_mouse_indev.data_frame;
    }
}



void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    /*-----------------------------
     * Create a buffer for drawing
     *----------------------------*/

    /**
     * LVGL requires a buffer where it internally draws the widgets.
     * Later this buffer will passed to your display driver's `flush_cb` to copy its content to your display.
     * The buffer has to be greater than 1 display row
     *
     * There are 3 buffering configurations:
     * 1. Create ONE buffer:
     *      LVGL will draw the display's content here and writes it to your display
     *
     * 2. Create TWO buffer:
     *      LVGL will draw the display's content to a buffer and writes it your display.
     *      You should use DMA to write the buffer's content to the display.
     *      It will enable LVGL to draw the next part of the screen to the other buffer while
     *      the data is being sent form the first buffer. It makes rendering and flushing parallel.
     *
     * 3. Double buffering
     *      Set 2 screens sized buffers and set disp_drv.full_refresh = 1.
     *      This way LVGL will always provide the whole rendered screen in `flush_cb`
     *      and you only need to change the frame buffer's address.
     */

    /* Example for 1) */
    // static lv_disp_draw_buf_t draw_buf_dsc_1;
    // static lv_color_t buf_1[MY_DISP_HOR_RES * 120];                          /*A buffer for 10 rows*/
    // lv_disp_draw_buf_init(&draw_buf_dsc_1, buf_1, NULL, MY_DISP_HOR_RES * MY_DISP_VER_RES);   /*Initialize the display buffer*/

    #if use_buf_dsc_2 

    /* Example for 2) */
    static lv_disp_draw_buf_t draw_buf_dsc_2;
    static lv_color_t buf_2_1[MY_DISP_HOR_RES * 24];                        /*A buffer for 10 rows*/
    static lv_color_t buf_2_2[MY_DISP_HOR_RES * 24];                        /*An other buffer for 10 rows*/
    lv_disp_draw_buf_init(&draw_buf_dsc_2, buf_2_1, buf_2_2, MY_DISP_HOR_RES * 24);   /*Initialize the display buffer*/

    /* Example for 3) also set disp_drv.full_refresh = 1 below*/
    #endif

    
#if use_buf_dsc_3
    static lv_disp_draw_buf_t draw_buf_dsc_3;
    #if USE_PSRAM_FOR_BUFFER    /* Try to get buffer from PSRAM */
        static lv_color_t *buf_3_1 = (lv_color_t *)heap_caps_malloc(MY_DISP_VER_RES * MY_DISP_HOR_RES * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);/*A screen sized buffer*/
        static lv_color_t *buf_3_2 = (lv_color_t *)heap_caps_malloc(MY_DISP_VER_RES * MY_DISP_HOR_RES * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);/*Another screen sized buffer*/

        if ((buf_3_1 == NULL) || (buf_3_2 == NULL))
        {
            ESP_LOGE(TAG, "malloc buffer from PSRAM fialed");
            while (1)
                ;
        }
        else
        {
            ESP_LOGI(TAG, "malloc buffer from PSRAM successful");
        }
        lv_disp_draw_buf_init(&draw_buf_dsc_3, buf_3_1, buf_3_2,
                          MY_DISP_HOR_RES * MY_DISP_VER_RES); /*Initialize the display buffer*/
    #else
        static lv_color_t *buf_3_1 = (lv_color_t *)heap_caps_malloc(MY_DISP_VER_RES * MY_DISP_HOR_RES * sizeof(lv_color_t)/7, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        static lv_color_t *buf_3_2 = (lv_color_t *)heap_caps_malloc(MY_DISP_VER_RES * MY_DISP_HOR_RES * sizeof(lv_color_t)/7, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        // static lv_color_t *buf_3_1 = (lv_color_t *)malloc(MY_DISP_HOR_RES * MY_DISP_VER_RES * sizeof(lv_color_t)/2);
        // static lv_color_t *buf_3_2 = (lv_color_t *)malloc(MY_DISP_HOR_RES * MY_DISP_VER_RES * sizeof(lv_color_t)/2);
      
    lv_disp_draw_buf_init(&draw_buf_dsc_3, buf_3_1, buf_3_2,
                          MY_DISP_HOR_RES * MY_DISP_VER_RES/7); /*Initialize the display buffer*/
    #endif 
#endif
    print_ram_info();
    

    // static lv_disp_draw_buf_t draw_buf_dsc_2;
    // static lv_color_t buf_2_1[MY_DISP_HOR_RES * 24];                        /*A buffer for 10 rows*/
    // static lv_color_t buf_2_2[MY_DISP_HOR_RES * 24];                        /*An other buffer for 10 rows*/
    // lv_disp_draw_buf_init(&draw_buf_dsc_2, buf_2_1, buf_2_2, MY_DISP_HOR_RES * 24);   /*Initialize the display buffer*/

    /*-----------------------------------
     * Register the display in LVGL
     *----------------------------------*/

    static lv_disp_drv_t disp_drv; /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv);   /*Basic initialization*/

    /*Set up the functions to access to your display*/

    /*Set the resolution of the display*/
    disp_drv.hor_res = MY_DISP_HOR_RES;//width
    disp_drv.ver_res = MY_DISP_VER_RES;//height

    /*Used to copy the buffer's content to the display*/
    disp_drv.flush_cb = disp_flush;

    /*Set a display buffer*/
    // disp_drv.draw_buf = &draw_buf_dsc_1;
    #if use_buf_dsc_2
    disp_drv.draw_buf = &draw_buf_dsc_2;
    #endif
    #if use_buf_dsc_3
    disp_drv.draw_buf = &draw_buf_dsc_3;
    #endif
    /*Required for Example 3)*/

    disp_drv.full_refresh = 0; // 全屏幕刷新
    /* Set LVGL software rotation */
    //disp_drv.sw_rotate = 1;//软件旋转屏幕
    // disp_drv.rotated = LV_DISP_ROT_90;

    /* Fill a memory array with a color if you have GPU.
     * Note that, in lv_conf.h you can enable GPUs that has built-in support in LVGL.
     * But if you have a different GPU you can use with this callback.*/
    // disp_drv.gpu_fill_cb = gpu_fill;

    /*Finally register the driver*/
    lv_disp_drv_register(&disp_drv);
    ESP_LOGI(TAG, "register the driver");
}

static void disp_init(void)
{
#if USE_LGFX == 1
    ESP_LOGI(TAG, "[LVGL] LGFX lcd init...");
    tft.init();
    // tft.setRotation(1);
    tft.setBrightness(get_brightness_from_nvs());
    // tft.fillScreen(TFT_RED);
    // vTaskDelay(pdMS_TO_TICKS(1000));
    // tft.fillScreen(TFT_GREEN);
    // vTaskDelay(pdMS_TO_TICKS(1000));
    // tft.fillScreen(TFT_BLUE);
    // vTaskDelay(pdMS_TO_TICKS(1000));
    tft.fillScreen(TFT_BLACK);
#endif

#if USE_eTFT == 1
    Serial.println("[LVGL] eTFT lcd init...");
    tft = TFT_eSPI();
    tft.init();
    // tft.setRotation(2);
    tft.fillScreen(0xAD75);
#endif
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/*Flush the content of the internal buffer the specific area on the display
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_disp_flush_ready()' has to be called when finished.*/
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    // if(disp_flush_enabled) {
    /*The most simple case (but also the slowest) to put all pixels to the screen one-by-one*/

    // int32_t x;
    // int32_t y;
    // for(y = area->y1; y <= area->y2; y++) {
    //     for(x = area->x1; x <= area->x2; x++) {
    //         /*Put a pixel to the display. For example:*/
    //         /*put_px(x, y, *color_p)*/
    //         color_p++;
    //     }
    // }

    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

#if USE_LGFX == 1
    if (tft.getStartCount() == 0)
    {
        tft.startWrite();

        tft.setAddrWindow(area->x1, area->y1, w, h);
        tft.pushPixels((uint16_t *)&color_p->full, w * h, true);
        //tft.pushColors((uint16_t *)&color_p->full, w * h, true);  // 非 DMA 传输
        //tft.pushImageDMA(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
        // tft.waitDMA();
        tft.endWrite();
    }

#endif

#if USE_eTFT == 1
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushPixels((uint16_t *)&color_p->full, w * h);
    tft.endWrite();
#endif

    /*IMPORTANT!!!
     *Inform the graphics library that you are ready with the flushing*/
    lv_disp_flush_ready(disp_drv);
}



/**********************    lvgl ↑  *********************/
/**********************************************************/