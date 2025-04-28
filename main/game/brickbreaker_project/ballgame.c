#include "lvgl.h"
#include <stdlib.h>
#include "ballgame.h"
#include "esp_log.h"
extern lv_indev_t *indev_touchpad;

// 定义常量
#define  nhang     6       // 行数
#define  nlie      15      // 列数
#define  ballsizie   20     // 球的大小

// 定义方块结构体
typedef struct
{
    lv_obj_t  * obj;       // 方块对象
    lv_obj_t  * label;     // 方块标签
    unsigned int  alive;   // 方块生命值
}cubestype;

// 定义方块数组
cubestype  cube[nhang][nlie];


// 定义全局变量
static float movx=-1,movy=-1; // 球的移动速度
static float currentx, currenty; // 球的当前坐标
static int   endx; // 球的结束坐标
static lv_obj_t * arrow; // 箭头标签
static lv_obj_t * ball1; // 球对象
static lv_obj_t * board1; // 挡板对象
static lv_obj_t * slider; // 滑动条对象
static lv_obj_t * btn1; // 开始按钮
static lv_obj_t * lable; // 开始按钮标签
static lv_obj_t * btn2; // 退出按钮
static lv_obj_t * lable2; // 退出按钮标签
static lv_obj_t * qiu1; // 球对象
static lv_timer_t * t1; // 定时器
static lv_obj_t * panel; // 面板对象
static lv_obj_t * panellable; // 面板标签
static lv_obj_t * btext; // 分数标签
static lv_obj_t * screen1; // 屏幕对象
static lv_obj_t * botton_exit; // 退出按钮
static lv_obj_t * exit_lable; // 退出按钮标签
static lv_obj_t * label10; // 标签
static int score=0; // 分数
LV_IMG_DECLARE(qiu) // 声明球图片
// 定义函数原型
static void all_clear(lv_event_t * e);
static void btn1_event_cb(lv_event_t * e);
static void cube_anim_great(lv_color_t value,short x,short y);
static void timer_cb1(lv_timer_t * t);

// 定义屏幕宽高和方块宽高
static int screen_width;
static int screen_height;
static int cubewidht;
static int cubehight;

// 开始游戏函数
void ballgame_start()
{
    score=0; // 初始化分数
    
    // 创建屏幕对象
    screen1=lv_tileview_create(lv_scr_act());
    lv_obj_set_style_bg_color(screen1,lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_clear_flag(screen1, LV_OBJ_FLAG_SCROLLABLE);
    
    // Get screen resolution
    screen_width = lv_obj_get_width(lv_scr_act());
    screen_height = lv_obj_get_height(lv_scr_act());
    
    // Calculate cube width and height based on screen resolution
    cubewidht = screen_width / nlie;
    cubehight = (screen_height / 2) / nhang;
    
    btn1=lv_btn_create(screen1);
    lv_obj_set_align(btn1,LV_ALIGN_CENTER);
    lv_obj_set_size(btn1,150,50);
    
    lable=lv_label_create(btn1);
    lv_label_set_text(lable, "START GAME");
    lv_obj_set_align(lable,LV_ALIGN_CENTER);
    lv_obj_add_event_cb(btn1,btn1_event_cb,LV_EVENT_CLICKED,0);	
        
    botton_exit=lv_btn_create(screen1);
    lv_obj_set_style_bg_color(botton_exit,lv_color_hex(0x000040), LV_PART_MAIN);
    exit_lable=lv_label_create(botton_exit);
    lv_label_set_text(exit_lable, "<EXIT");
    lv_obj_set_style_text_color(exit_lable,lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_add_event_cb(botton_exit,all_clear,LV_EVENT_RELEASED,0);
    
    t1=lv_timer_create(timer_cb1,6,0);
    lv_timer_pause(t1);	
}

void exec_cbx1(void * var, int32_t v)
{
    lv_obj_t * xxx=(lv_obj_t *)var;
    lv_obj_set_x(xxx,lv_obj_get_x(xxx)-1);
    lv_obj_set_y(xxx,lv_obj_get_y(xxx)+v-1);	
}

void exec_cbx2(void * var, int32_t v)
{
    lv_obj_t * xxx=(lv_obj_t *)var;
    lv_obj_set_x(xxx,lv_obj_get_x(xxx)+1);
    lv_obj_set_y(xxx,lv_obj_get_y(xxx)+v-1);
}

void exec_cbx3(void * var, int32_t v)
{
    lv_obj_t * xxx=(lv_obj_t *)var;
    lv_obj_set_x(xxx,lv_obj_get_x(xxx)-3);
    lv_obj_set_y(xxx,lv_obj_get_y(xxx)+v-1);
}

void exec_cbx4(void * var, int32_t v)
{
    lv_obj_t * xxx=(lv_obj_t *)var;
    lv_obj_set_x(xxx,lv_obj_get_x(xxx)+3);
    lv_obj_set_y(xxx,lv_obj_get_y(xxx)+v-1);
}

void exec_cbx5(void * var, int32_t v)
{
    lv_obj_t * xxx=(lv_obj_t *)var;
    lv_obj_set_x(xxx,lv_obj_get_x(xxx)+5);
    lv_obj_set_y(xxx,lv_obj_get_y(xxx)+v-1);
}

void exec_cbx6(void * var, int32_t v)
{
    lv_obj_t * xxx=(lv_obj_t *)var;
    lv_obj_set_x(xxx,lv_obj_get_x(xxx)+-5);
    lv_obj_set_y(xxx,lv_obj_get_y(xxx)+v-1);
}

void ready_cb( lv_anim_t * var)
{
    lv_obj_del((lv_obj_t *)var->var);
}

void cube_anim_great(lv_color_t value,short x,short y)
{
    int i;
    lv_anim_t a[6];
    lv_obj_t * cube_split[6];
    lv_anim_exec_xcb_t lv_anim_exec_xcb[6]={exec_cbx1,exec_cbx2,exec_cbx3,exec_cbx4,exec_cbx5,exec_cbx6,};
    
    for(int i=0;i<6;i++)
    {
        cube_split[i]=lv_btn_create(screen1);
        lv_obj_set_size(cube_split[i],(cubewidht-2)/2,(cubehight-2)/2);
        lv_obj_set_pos(cube_split[i],cubewidht/4+x,cubehight/4+y);
        lv_obj_set_style_bg_color(cube_split[i],value, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(cube_split[i],0,LV_PART_MAIN);
        lv_obj_refr_pos(cube_split[i]);		

        lv_anim_init(&a[i]);
        lv_anim_set_var(&a[i],cube_split[i]);
        lv_anim_set_exec_cb(&a[i],lv_anim_exec_xcb[i]);
        lv_anim_set_time(&a[i],1500);
        lv_anim_set_delay(&a[i], rand()>>24);
        lv_anim_set_values(&a[i],1,20+(rand()>>27));
        lv_anim_set_ready_cb(&a[i], ready_cb);	
        lv_anim_start(&a[i]);		
    }
}

static int obj_is_crash(lv_obj_t  * obj1,lv_obj_t  * obj2)
{
    int x1,y1,x2,y2,w1,w2,h1,h2,dx,dy;
    
    x1=lv_obj_get_x(obj1);
    x2=lv_obj_get_x(obj2);
    
    y1=lv_obj_get_y(obj1);
    y2=lv_obj_get_y(obj2);
    
    w1=lv_obj_get_width(obj1);
    w2=lv_obj_get_width(obj2);
    
    h1=lv_obj_get_height(obj1);
    h2=lv_obj_get_height(obj2);
    
    if((x2-x1)==w1&&(y2-y1)<=(h1-3)&&(y1-y2)<=h2-3){return 1;}
    if((y1-y2)==h2&&(x2-x1)<=(w1-3)&&(x1-x2)<=w2-3){return 2;}
    if((x1-x2)==w2&&(y2-y1)<=(h1-3)&&(y1-y2)<=h2-3){return 3;}
    if((y2-y1)==h1&&(x2-x1)<=(w1-3)&&(x1-x2)<=w2-3){return 4;}
    return 0;
}

void slider1_event_cb(lv_event_t * e)
{
    lv_obj_t * tager=lv_event_get_target(e);
    lv_obj_t * usertarget=(lv_obj_t *)lv_event_get_user_data(e);
    
    unsigned int value=lv_slider_get_value(tager);
    lv_obj_set_x(usertarget,value);
}

void timer_cb1(lv_timer_t * t)
{
    //lv_indev_t * indev_touchpad = lv_indev_get_act();
    lv_point_t vect;
    lv_indev_get_vect(indev_touchpad, &vect);
    
    int qiu_board_distan=0;
    static int qiuangle=0;
    static int last_touchx = 0; // 记录上一次的触摸点
	int touchx = vect.x;
    //ESP_LOGI("GAME", "touchx:%d", touchx);
	if (touchx <= screen_width - 100 && touchx >= 0)
    {
        int delta = touchx - last_touchx; // 计算移动距离
        int step = delta / 5; // 限制移动速度
        int new_x = lv_obj_get_x(board1) + step;

        // 限制挡板移动范围
        if (new_x < 0) new_x = 0;
        if (new_x > screen_width - 100) new_x = screen_width - 100;

        lv_obj_set_x(board1, new_x);
        last_touchx = touchx; // 更新上一次的触摸点
    }
    if(qiuangle>=3600){qiuangle=0;}
    
    lv_label_set_text_fmt(panellable,"score%d",score);
    
    if(touchx <= screen_width - 100 && touchx >= 0)
    {
        lv_obj_set_x(board1, touchx);
    }
    
    currentx += movx;
    endx = currentx;
    currenty += movy;
    qiuangle += 40 * movx;
    lv_obj_set_pos(qiu1, endx, currenty);
    lv_img_set_angle(qiu1, qiuangle);
    
    if(currenty > screen_height - ballsizie)
    {
        all_clear(0);
        return;
    }

    if(obj_is_crash(qiu1, board1) == 4)
    {
        qiu_board_distan = lv_obj_get_x(qiu1) - lv_obj_get_x(board1) - 40;
        movx = (float)qiu_board_distan / 50;
        if(movx < -1) movx = -1;
        if(movx > 1) movx = 1;
        movy = movy * (-1);
        currenty += movy;
    }
    
    if(endx <= 0 || endx >= screen_width - ballsizie)
    {
        endx -= movx;
        currenty -= movy;
        movx = movx * (-1);
    }
    if(currenty <= 0 || currenty >= screen_height - ballsizie)
    {
        endx -= movx;
        currenty -= movy;
        movy = movy * (-1);
    }
    
    for(int i = 0; i < nhang; i++)
    {
        for(int j = 0; j < nlie; j++)
        {
            if(cube[i][j].alive)
            {
                if(obj_is_crash(qiu1, cube[i][j].obj) == 1)
                {
                    score++;
                    cube_anim_great(lv_obj_get_style_bg_color(cube[i][j].obj, LV_PART_MAIN), lv_obj_get_x(cube[i][j].obj), lv_obj_get_y(cube[i][j].obj));
                    cube[i][j].alive--;
                    lv_label_set_text_fmt(cube[i][j].label, "%d", cube[i][j].alive);
                    currentx -= movx;
                    currenty -= movy;
                    movx = movx * (-1);
                    if(cube[i][j].alive == 0)
                    {
                        lv_obj_del(cube[i][j].obj);
                    }
                    return;
                }
                
                if(obj_is_crash(qiu1, cube[i][j].obj) == 2)
                {
                    score++;
                    cube_anim_great(lv_obj_get_style_bg_color(cube[i][j].obj, LV_PART_MAIN), lv_obj_get_x(cube[i][j].obj), lv_obj_get_y(cube[i][j].obj));
                    cube[i][j].alive--;
                    lv_label_set_text_fmt(cube[i][j].label, "%d", cube[i][j].alive);
                    currentx -= movx;
                    currenty -= movy;
                    movy = movy * (-1);
                    if(cube[i][j].alive == 0)
                    {
                        lv_obj_del(cube[i][j].obj);
                    }
                    return;
                }
                
                if(obj_is_crash(qiu1, cube[i][j].obj) == 3)
                {
                    score++;
                    cube_anim_great(lv_obj_get_style_bg_color(cube[i][j].obj, LV_PART_MAIN), lv_obj_get_x(cube[i][j].obj), lv_obj_get_y(cube[i][j].obj));
                    cube[i][j].alive--;
                    lv_label_set_text_fmt(cube[i][j].label, "%d", cube[i][j].alive);
                    currentx -= movx;
                    currenty -= movy;
                    movx = movx * (-1);
                    if(cube[i][j].alive == 0)
                    {
                        lv_obj_del(cube[i][j].obj);
                    }
                    return;
                }
                
                if(obj_is_crash(qiu1, cube[i][j].obj) == 4)
                {
                    score++;
                    cube_anim_great(lv_obj_get_style_bg_color(cube[i][j].obj, LV_PART_MAIN), lv_obj_get_x(cube[i][j].obj), lv_obj_get_y(cube[i][j].obj));
                    cube[i][j].alive--;
                    lv_label_set_text_fmt(cube[i][j].label, "%d", cube[i][j].alive);
                    currentx -= movx;
                    currenty -= movy;
                    movy = movy * (-1);
                    if(cube[i][j].alive == 0)
                    {
                        lv_obj_del(cube[i][j].obj);
                    }
                    return;
                }
            }
        }
    }
    
    for(int i = 0; i < nhang; i++)
    {
        for(int j = 0; j < nlie; j++)
        {
            if(cube[i][j].alive)
            {
                return;
            }
        }
    }
    
    lv_obj_del(slider);
    lv_obj_del(board1);
    lv_obj_del(qiu1);
    lv_obj_del(panellable);
    lv_timer_del(t1);
    ballgame_start();
}

void btn1_event_cb(lv_event_t * e)
{
    currentx = screen_width / 2;
    currenty = screen_height / 2;
    lv_obj_del(btn1);
    lv_obj_set_style_bg_color(screen1, lv_color_hex(0x000000), LV_PART_MAIN);
    
    for(int i = 0; i < nhang; i++)
    {
        for(int j = 0; j < nlie; j++)
        {
            cube[i][j].obj = lv_btn_create(screen1);
            lv_obj_set_size(cube[i][j].obj, cubewidht - 2, cubehight - 2);
            lv_obj_set_pos(cube[i][j].obj, j * cubewidht, i * cubehight);
            lv_obj_set_style_bg_color(cube[i][j].obj, lv_color_hex(rand() * 512 + rand() / 128), LV_PART_MAIN);
            lv_obj_set_style_radius(cube[i][j].obj, 0, LV_PART_MAIN);
            lv_obj_set_style_shadow_width(cube[i][j].obj, 0, LV_PART_MAIN);
            cube[i][j].alive = (rand() >> 28) + 1;
            cube[i][j].label = lv_label_create(cube[i][j].obj);
            lv_obj_center(cube[i][j].label);
            lv_label_set_text_fmt(cube[i][j].label, "%d", cube[i][j].alive);
        }
    }
    
    board1 = lv_btn_create(screen1);
    lv_obj_set_align(board1, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(board1, -30);
    lv_obj_set_size(board1, 100, 14);
    lv_obj_set_style_bg_color(board1, lv_color_hex(0xff0000), LV_PART_MAIN);

    arrow = lv_label_create(board1);
    lv_label_set_text(arrow, "<<<<<==>>>>>");
    lv_obj_set_style_text_color(arrow, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_center(arrow);
    
    qiu1 = lv_img_create(screen1);
    lv_img_set_src(qiu1, &qiu);

    score = 0;
    panellable = lv_label_create(screen1);
    lv_label_set_text(panellable, "START GAME");
    lv_obj_set_style_text_color(panellable, lv_color_hex(0x00ffff), LV_PART_MAIN);
    lv_obj_set_align(panellable, LV_ALIGN_TOP_MID);

    lv_timer_resume(t1);
}

void all_clear(lv_event_t * e)
{
    lv_timer_del(t1);
    lv_anim_del_all();
    lv_obj_clean(screen1);
    lv_obj_del(screen1);
}