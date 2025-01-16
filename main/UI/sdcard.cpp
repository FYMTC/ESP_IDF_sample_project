#include "sdcard.hpp"
void file_item_click_handler(lv_event_t *e);
void back_btn_click_handler(lv_event_t *e);
void all_clear(lv_event_t *e);
const char *get_file_extension(const char *filename); // 获取文件扩展名
static void close_event_handler(lv_event_t *e);
static const char *TAG = "SD_CARD_FILE_BROWSER";
static void show_text_content(const char *text);
static void show_image(const char *path);
#define MAX_PIC_FILE_SIZE 1024 * 10
#define MAX_TXT_FILE_SIZE 1024 * 1
#define MAX_PATH_LENGTH 256

static char current_path[256] = sdcard_mount_point; // 记录当前路径
static lv_obj_t *file_list;                         // 文件列表对象
static lv_obj_t *back_btn;                          // 返回按钮

// 列出目录中的文件和文件夹
void list_files(const char *dir_path)
{
    DIR *dir = opendir(dir_path);
    if (dir == NULL)
    {
        ESP_LOGE(TAG, "Failed to open directory");
        return;
    }

    lv_obj_clean(file_list); // 清空文件列表

    // 添加返回按钮
    if (strcmp(current_path, sdcard_mount_point) != 0)
    {
        lv_obj_t *list_btn = lv_list_add_btn(file_list, LV_SYMBOL_BACKSPACE, current_path);
        lv_obj_add_event_cb(list_btn, back_btn_click_handler, LV_EVENT_CLICKED, NULL);
    }
    else
    {
        lv_obj_t *list_btn = lv_list_add_btn(file_list, LV_SYMBOL_NEW_LINE, "back");
        lv_obj_add_event_cb(list_btn, all_clear, LV_EVENT_CLICKED, NULL);
    }

    struct dirent *entry;
    // 遍历目录
    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_name[0] == '.')
        {
            // 忽略 '.' 和 '..'
            continue;
        }

        char full_path[MAX_PATH_LENGTH];
        // size_t required_size = snprintf(nullptr, 0, "%s/%s", dir_path, entry->d_name) + 1; // 计算所需大小
        // if (required_size > sizeof(full_path))
        // {
        //     printf("Path too long: %s/%s\n", dir_path, entry->d_name);
        //     return;
        // }
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        // printf("Original file name: %s\n", entry->d_name);  // 输出原始文件名

        struct stat st;
        if (stat(full_path, &st) == 0)
        {
            lv_obj_t *list_btn;
            const char *icon = LV_SYMBOL_FILE; // 默认图标
            // 根据文件类型选择不同的图标
            if (S_ISDIR(st.st_mode))
            {
                // 文件夹
                icon = LV_SYMBOL_DIRECTORY;
            }
            else
            {
                // 根据文件扩展名设置图标
                const char *ext = get_file_extension(entry->d_name);
                if (strcmp(ext, "txt") == 0)
                {
                    icon = LV_SYMBOL_EDIT;
                }
                else if (strcmp(ext, "jpg") == 0 || strcmp(ext, "png") == 0)
                {
                    icon = LV_SYMBOL_IMAGE;
                }
                else if (strcmp(ext, "mp3") == 0 || strcmp(ext, "wav") == 0)
                {
                    icon = LV_SYMBOL_AUDIO;
                }
                else if (strcmp(ext, "mp4") == 0 || strcmp(ext, "avi") == 0)
                {
                    icon = LV_SYMBOL_VIDEO;
                }
                // 添加更多文件类型的判断
            }

            // 添加按钮到列表
            list_btn = lv_list_add_btn(file_list, icon, entry->d_name);
            lv_obj_add_event_cb(list_btn, file_item_click_handler, LV_EVENT_CLICKED, NULL);
        }
    }

    closedir(dir);
}
// 获取文件扩展名
const char *get_file_extension(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename)
    {
        return ""; // 没有扩展名
    }
    return dot + 1;
}
// 文件项点击事件处理函数
void file_item_click_handler(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    const char *file_name = lv_list_get_btn_text(file_list, btn);

    char full_path[MAX_PATH_LENGTH];
    snprintf(full_path, sizeof(full_path), "%s/%s", current_path, file_name);
    ESP_LOGI(TAG, "File selected: %s", full_path);

    // 获取文件信息
    struct stat st;
    if (stat(full_path, &st) != 0)
    {
        ESP_LOGE(TAG, "Failed to get file info: %s", full_path);
        return;
    }
    // 为防止栈溢出，将 struct stat 分配到堆上
    // struct stat *st = (struct stat *)malloc(sizeof(struct stat));

    // if (!st)
    // {
    //     ESP_LOGE(TAG, "Failed to allocate memory for struct stat");
    //     return;
    // }
    // if (stat(full_path, st) != 0)
    // {
    //     ESP_LOGE(TAG, "Failed to get file info: %s", full_path);
    //     return;
    // }

    // 如果是目录，则进入目录
    // if (S_ISDIR(st->st_mode))
    // {
    //     // 更新当前路径
    //     snprintf(current_path, sizeof(current_path), "%s", full_path);
    //     list_files(current_path);
    //     ESP_LOGI(TAG, "current_path: %s", current_path);
    //     return;
    // }
    if (S_ISDIR(st.st_mode))
    {
        snprintf(current_path, sizeof(current_path), "%s", full_path);
        list_files(current_path);
        ESP_LOGI(TAG, "Current path: %s", current_path);
        return;
    }

    // 获取文件扩展名
    const char *ext = strrchr(file_name, '.');
    if (!ext)
    {
        ESP_LOGI(TAG, "File has no extension: %s", full_path);
        return;
    }
    ext++; // 跳过 '.'

    // 处理文本文件
    if (strcmp(ext, "txt") == 0 || strcmp(ext, "lrc") == 0)
    {
        // 检查文件大小
        // if (st->st_size > MAX_TXT_FILE_SIZE)
        // { // 文件大于 1KB，不处理
        //     ESP_LOGI(TAG, "File is too large: %s (size: %ld bytes)", full_path, st->st_size);
        //     free(st); // 释放内存
        //     return;
        // }
        if (st.st_size > MAX_TXT_FILE_SIZE)
        {
            ESP_LOGI(TAG, "File is too large: %s (size: %ld bytes)", full_path, st.st_size);
            return;
        }
        FILE *f = fopen(full_path, "r");
        if (!f)
        {
            ESP_LOGE(TAG, "Failed to open file: %s", full_path);
            // free(st); // 释放内存
            return;
        }

        char content[MAX_TXT_FILE_SIZE] = {0}; // 用于存储文件内容
        size_t len = fread(content, 1, sizeof(content) - 1, f);
        fclose(f);

        if (len > 0)
        {
            content[len] = '\0'; // 确保字符串以 null 结尾
            //ESP_LOGI(TAG, "File content: %s", content);
            show_text_content(content); // 在 LVGL 中显示文本
        }
        else
        {
            ESP_LOGE(TAG, "Failed to read file: %s", full_path);
        }
    }
    // 处理图片文件
    else if (strcmp(ext, "jpg") == 0 || strcmp(ext, "png") == 0)
    {
        // 检查文件大小
        // if (st->st_size > MAX_PIC_FILE_SIZE)
        // { // 文件大于 1KB，不处理
        //     ESP_LOGI(TAG, "File is too large: %s (size: %ld bytes)", full_path, st->st_size);
        //     free(st); // 释放内存
        //     return;
        // }
        if (st.st_size > MAX_PIC_FILE_SIZE)
        {
            ESP_LOGI(TAG, "File is too large: %s (size: %ld bytes)", full_path, st.st_size);
            return;
        }
        show_image(full_path); // 在 LVGL 中显示图片
    }
    // 其他文件不处理
    else
    {
        ESP_LOGI(TAG, "Unsupported file type: %s, open as text", full_path);
        // 检查文件大小
        // if (st->st_size > MAX_TXT_FILE_SIZE)
        // { // 文件大于 1KB，不处理
        //     ESP_LOGI(TAG, "File is too large: %s (size: %ld bytes)", full_path, st->st_size);
        //     free(st); // 释放内存
        //     return;
        // }
        if (st.st_size > MAX_TXT_FILE_SIZE)
        {
            ESP_LOGI(TAG, "File is too large: %s (size: %ld bytes)", full_path, st.st_size);
            return;
        }
        FILE *f = fopen(full_path, "r");
        if (!f)
        {
            ESP_LOGE(TAG, "Failed to open file: %s", full_path);
            // free(st); // 释放内存
            return;
        }
        // 用于存储文件内容
        #if USE_PSRAM_FOR_BUFFER
        
        char *content = (char *)heap_caps_malloc(MAX_TXT_FILE_SIZE,MALLOC_CAP_SPIRAM);
        if (!content)
        {
            ESP_LOGE(TAG, "Failed to allocate memory in PSRAM for content");
            return;
        }
        #else
        content[1024] = {0}; 
        #endif
        size_t len = fread(content, 1, sizeof(content) - 1, f);
        fclose(f);

        if (len > 0)
        {
            content[len] = '\0'; // 确保字符串以 null 结尾
            //ESP_LOGI(TAG, "File content: %s", content);
            show_text_content(content); // 在 LVGL 中显示文本
        }
        else
        {
            ESP_LOGE(TAG, "Failed to read file: %s", full_path);
        }
    }

    // free(st);        // 释放内存
    // free(full_path); // 释放内存
}
// 显示文本内容的回调函数
static void show_text_content(const char *text)
{
    // lv_obj_t* text_area = lv_textarea_create(lv_scr_act());
    // lv_textarea_set_text(text_area, text);
    // lv_obj_set_size(text_area, LV_PCT(80), LV_PCT(80));
    // lv_obj_align(text_area, LV_ALIGN_CENTER, 0, 0);

    // 创建窗口
    lv_obj_t *win = lv_win_create(lv_scr_act(), 30);
    lv_obj_set_size(win, LV_HOR_RES, LV_VER_RES);
    lv_win_add_title(win, current_path);
    lv_obj_t *btn = lv_win_add_btn(win, LV_SYMBOL_CLOSE, 90);
    lv_obj_add_event_cb(btn, close_event_handler, LV_EVENT_CLICKED, win);
    // 创建文本
    lv_obj_t *cont = lv_win_get_content(win);
    lv_obj_t *label = lv_label_create(cont);
    lv_obj_set_width(label, LV_PCT(100));
    // lv_obj_set_style_text_font(label, &NotoSansSC_Medium_3500, 0);
    lv_label_set_text(label, text);
}
static void close_event_handler(lv_event_t *e)
{
    lv_obj_t *obj = (lv_obj_t *)e->user_data;
    // lv_obj_t * close = lv_obj_get_parent(obj);
    lv_obj_del(obj);
    // dawlist();
}
static void drag_event_handler(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);

    lv_indev_t * indev = lv_indev_get_act();
    if(indev == NULL)  return;

    lv_point_t vect;
    lv_indev_get_vect(indev, &vect);

    lv_coord_t x = lv_obj_get_x(obj) + vect.x;
    lv_coord_t y = lv_obj_get_y(obj) + vect.y;
    lv_obj_set_pos(obj, x, y);
    
}


// 显示图片的回调函数
static void show_image(const char *path)
{
    lv_obj_t *win = lv_win_create(lv_scr_act(), LV_PCT(10));
    lv_obj_set_size(win, LV_HOR_RES, LV_VER_RES);
    lv_win_add_title(win, path);
    lv_obj_t *btn = lv_win_add_btn(win, LV_SYMBOL_CLOSE, LV_PCT(50));
    lv_obj_add_event_cb(btn, close_event_handler, LV_EVENT_CLICKED, win);
    lv_obj_t *obj = lv_obj_create(win);
    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
    lv_obj_t *img = lv_img_create(obj);
    
    lv_img_set_antialias(img, true);
    //lv_img_set_pivot(img, 0, 0);
    lv_img_set_size_mode(img, LV_IMG_SIZE_MODE_VIRTUAL);
    lv_obj_center(img);
    //lv_obj_add_event_cb(img, drag_event_handler, LV_EVENT_PRESSING, NULL);
    lv_img_set_src(img, path);
    ESP_LOGI(TAG, "File img lv_full_path: %s", path);
}
// 返回按钮点击事件
void back_btn_click_handler(lv_event_t *e)
{
    if (strcmp(current_path, sdcard_mount_point) != 0)
    {
        // 返回到上级目录
        char *last_slash = strrchr(current_path, '/');
        if (last_slash != NULL)
        {
            *last_slash = '\0'; // 去掉当前目录名称，回到上级目录
            list_files(current_path);
        }
    }
}

// 创建文件浏览器界面
void create_file_browser_ui()
{
    file_list = lv_list_create(lv_scr_act());
    lv_obj_set_size(file_list, LV_PCT(100), LV_PCT(100));
    lv_obj_align(file_list, LV_ALIGN_CENTER, 0, 0);

    // 列出当前路径的文件
    list_files(current_path);
}

void all_clear(lv_event_t *e)
{
    // lv_timer_del(t);
    // all_init();
    // lv_obj_clean(screen);
    // lv_obj_del(screen);

    lv_obj_clean(file_list);
    lv_obj_del(file_list);
}