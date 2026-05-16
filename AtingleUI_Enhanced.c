#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>
#include <glib.h>
#include <limits.h>
#include <curl/curl.h>
#include <json-c/json.h>

static GtkWidget *main_window;
static GtkWidget *editor;
static GtkWidget *status_label;
static GtkWidget *attach_button;
static GtkWidget *notebook;
static GtkWidget *script_hub_view;

// CURL response structure
typedef struct {
    char *ptr;
    size_t len;
} CurlResponse;

pid_t findPID(const char *target_name) {
    DIR* d = opendir("/proc");
    if (!d) {
        return -1;
    }

    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_type != DT_DIR) continue;

        pid_t pid = (pid_t)atoi(e->d_name);
        if (pid <= 0) continue;

        char exe_path[PATH_MAX];
        char link_target[PATH_MAX];
        snprintf(exe_path, sizeof(exe_path), "/proc/%d/exe", pid);

        ssize_t len = readlink(exe_path, link_target, sizeof(link_target) - 1);
        if (len == -1) continue;
        link_target[len] = '\0';

        if (strstr(link_target, target_name)) {
            closedir(d);
            return pid;
        }
    }
    closedir(d);
    return -1;
}

char* load_file_to_string(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    rewind(f);
    
    if (length < 0) {
        fclose(f);
        return NULL;
    }
    
    char *buffer = (char*)malloc((size_t)length + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }
    
    size_t read_size = fread(buffer, 1, (size_t)length, f);
    buffer[read_size] = '\0';
    fclose(f);
    return buffer;
}

// CURL callback for response handling
static size_t curl_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    CurlResponse *mem = (CurlResponse *)userp;

    char *ptr = realloc(mem->ptr, mem->len + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "Not enough memory for CURL response\n");
        return 0;
    }

    mem->ptr = ptr;
    memcpy(&(mem->ptr[mem->len]), contents, realsize);
    mem->len += realsize;
    mem->ptr[mem->len] = 0;

    return realsize;
}

// Fetch scripts from ScriptBlox API
static char* fetch_scripts_from_scriptblox(const char *category) {
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    CurlResponse response = {malloc(1), 0};
    if (!response.ptr) {
        curl_easy_cleanup(curl);
        return NULL;
    }

    char url[512];
    snprintf(url, sizeof(url), "https://scriptblox.com/api/script/search?category=%s&limit=20", category);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(response.ptr);
        return NULL;
    }

    return response.ptr;
}

// Parse JSON response and populate script list
static void populate_script_hub(const char *json_response, GtkWidget *list_box) {
    if (!json_response) return;

    struct json_object *root = json_tokener_parse(json_response);
    if (!root) return;

    struct json_object *scripts = NULL;
    json_object_object_get_ex(root, "scripts", &scripts);

    if (!scripts || !json_object_is_type(scripts, json_type_array)) {
        json_object_put(root);
        return;
    }

    int array_len = json_object_array_length(scripts);
    for (int i = 0; i < array_len; i++) {
        struct json_object *script = json_object_array_get_idx(scripts, i);
        struct json_object *title_obj, *desc_obj, *id_obj;

        json_object_object_get_ex(script, "title", &title_obj);
        json_object_object_get_ex(script, "description", &desc_obj);
        json_object_object_get_ex(script, "id", &id_obj);

        const char *title = json_object_get_string(title_obj);
        const char *description = json_object_get_string(desc_obj);
        const char *id = json_object_get_string(id_obj);

        if (title) {
            GtkWidget *row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
            gtk_widget_set_margin_top(row, 5);
            gtk_widget_set_margin_bottom(row, 5);
            gtk_widget_set_margin_start(row, 10);
            gtk_widget_set_margin_end(row, 10);

            GtkWidget *title_label = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(title_label), g_strdup_printf("<b>%s</b>", title));
            gtk_label_set_xalign(GTK_LABEL(title_label), 0.0);
            gtk_box_append(GTK_BOX(row), title_label);

            if (description) {
                GtkWidget *desc_label = gtk_label_new(description);
                gtk_label_set_wrap(GTK_LABEL(desc_label), TRUE);
                gtk_label_set_xalign(GTK_LABEL(desc_label), 0.0);
                gtk_box_append(GTK_BOX(row), desc_label);
            }

            GtkWidget *btn_load = gtk_button_new_with_label("Load Script");
            g_object_set_data_full(G_OBJECT(btn_load), "script_id", g_strdup(id ? id : ""), g_free);
            gtk_box_append(GTK_BOX(row), btn_load);

            gtk_list_box_append(GTK_LIST_BOX(list_box), row);
        }
    }

    json_object_put(root);
}

// Thread function to fetch scripts
struct fetch_thread_data {
    char *category;
    GtkWidget *list_box;
};

static void* fetch_scripts_thread(void *arg) {
    struct fetch_thread_data *data = (struct fetch_thread_data *)arg;
    
    char *response = fetch_scripts_from_scriptblox(data->category);
    if (response) {
        g_idle_add((GSourceFunc)populate_script_hub, response);
    }

    g_free(data->category);
    g_free(data);
    return NULL;
}

static void on_category_changed(GtkComboBoxText *combo, gpointer user_data) {
    GtkWidget *list_box = GTK_WIDGET(user_data);
    
    // Clear existing items
    GtkWidget *child = gtk_widget_get_first_child(list_box);
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(list_box), child);
        child = next;
    }

    const char *category = gtk_combo_box_text_get_active_text(combo);
    if (!category) category = "roblox";

    struct fetch_thread_data *data = g_malloc(sizeof(struct fetch_thread_data));
    data->category = g_strdup(category);
    data->list_box = list_box;

    pthread_t thread;
    pthread_create(&thread, NULL, fetch_scripts_thread, data);
    pthread_detach(thread);
}

static void on_load_script_clicked(GtkButton *button, gpointer user_data) {
    (void)user_data;
    const char *script_id = g_object_get_data(G_OBJECT(button), "script_id");
    if (script_id) {
        // Fetch full script content from ScriptBlox API
        CURL *curl = curl_easy_init();
        if (curl) {
            CurlResponse response = {malloc(1), 0};
            char url[512];
            snprintf(url, sizeof(url), "https://scriptblox.com/api/script/%s", script_id);
            
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
            
            CURLcode res = curl_easy_perform(curl);
            if (res == CURLE_OK && response.ptr) {
                struct json_object *root = json_tokener_parse(response.ptr);
                struct json_object *script_obj;
                json_object_object_get_ex(root, "script", &script_obj);
                
                if (script_obj) {
                    struct json_object *source_obj;
                    json_object_object_get_ex(script_obj, "source", &source_obj);
                    const char *source = json_object_get_string(source_obj);
                    
                    if (source) {
                        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editor));
                        gtk_text_buffer_set_text(buffer, source, -1);
                        gtk_label_set_text(GTK_LABEL(status_label), "Script loaded from ScriptBlox!");
                        
                        // Switch to editor tab
                        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);
                    }
                }
                json_object_put(root);
            }
            free(response.ptr);
            curl_easy_cleanup(curl);
        }
    }
}

static void on_execute_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editor));
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    gchar *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    
    if (text && strlen(text) > 0) {
        g_print("[EXECUTE] Script executed:\n%s\n", text);
        gtk_label_set_text(GTK_LABEL(status_label), "Script executed");
    } else {
        gtk_label_set_text(GTK_LABEL(status_label), "No script to execute");
    }
    g_free(text);
}

static void on_clear_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    (void)user_data;
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editor));
    gtk_text_buffer_set_text(buffer, "", -1);
    gtk_label_set_text(GTK_LABEL(status_label), "Editor cleared");
}

static void open_file_response(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    (void)user_data;
    
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    GFile *file = gtk_file_dialog_open_finish(dialog, res, NULL);
    
    if (file) {
        char *filename = g_file_get_path(file);
        if (filename) {
            char *content = load_file_to_string(filename);
            if (content) {
                GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editor));
                gtk_text_buffer_set_text(buffer, content, -1);
                gtk_label_set_text(GTK_LABEL(status_label), "File loaded successfully");
                free(content);
            } else {
                gtk_label_set_text(GTK_LABEL(status_label), "Failed to load file");
            }
            g_free(filename);
        }
        g_object_unref(file);
    }
}

static void on_open_file_clicked(GtkButton *button, gpointer user_data) {
    (void)user_data;
    
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open Script");
    gtk_file_dialog_open(dialog, GTK_WINDOW(main_window), NULL, open_file_response, NULL);
}

static void save_file_response(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    (void)user_data;
    
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    GFile *file = gtk_file_dialog_save_finish(dialog, res, NULL);
    
    if (file) {
        char *filename = g_file_get_path(file);
        if (filename) {
            GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editor));
            GtkTextIter start, end;
            gtk_text_buffer_get_start_iter(buffer, &start);
            gtk_text_buffer_get_end_iter(buffer, &end);
            gchar *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
            
            FILE *f = fopen(filename, "w");
            if (f) {
                fwrite(text, 1, strlen(text), f);
                fclose(f);
                gtk_label_set_text(GTK_LABEL(status_label), "File saved successfully");
            } else {
                gtk_label_set_text(GTK_LABEL(status_label), "Failed to save file");
            }
            g_free(text);
            g_free(filename);
        }
        g_object_unref(file);
    }
}

static void on_save_file_clicked(GtkButton *button, gpointer user_data) {
    (void)user_data;
    
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Save Script");
    gtk_file_dialog_save(dialog, GTK_WINDOW(main_window), NULL, save_file_response, NULL);
}

struct attach_data {
    GtkButton *button;
};

struct label_update_data {
    GtkLabel *label;
    GtkButton *button;
    char *new_label;
    char *status_msg;
};

static gboolean update_button_and_status(gpointer user_data) {
    struct label_update_data *data = (struct label_update_data *)user_data;
    gtk_button_set_label(data->button, data->new_label);
    gtk_label_set_text(data->label, data->status_msg);
    gtk_widget_set_sensitive(GTK_WIDGET(data->button), TRUE);
    
    g_free(data->new_label);
    g_free(data->status_msg);
    g_free(data);
    return G_SOURCE_REMOVE;
}

static void* attach_thread_func(void *arg) {
    struct attach_data *data = (struct attach_data *)arg;
    const char* so_path = "./sober_test_inject.so";
    const char* injector_path = "./injector";

    if (access(injector_path, F_OK) == -1) {
        struct label_update_data *upd = g_new(struct label_update_data, 1);
        upd->button = data->button;
        upd->label = GTK_LABEL(status_label);
        upd->new_label = g_strdup("Attach");
        upd->status_msg = g_strdup("Error: injector binary not found");
        g_idle_add(update_button_and_status, upd);
        g_free(data);
        return NULL;
    }

    pid_t pid = findPID("sober");
    if (pid == -1) {
        struct label_update_data *upd = g_new(struct label_update_data, 1);
        upd->button = data->button;
        upd->label = GTK_LABEL(status_label);
        upd->new_label = g_strdup("Attach");
        upd->status_msg = g_strdup("Sober process not found");
        g_idle_add(update_button_and_status, upd);
        g_free(data);
        return NULL;
    }

    g_print("[INFO] Attempting to inject into PID %d\n", pid);

    gchar *stdout_buf = NULL;
    gchar *stderr_buf = NULL;
    gint exit_status = 0;
    GError *error = NULL;

    gchar *pid_str = g_strdup_printf("%d", pid);
    gchar *argv[] = { (gchar*)injector_path, pid_str, (gchar*)so_path, NULL };

    gboolean success = g_spawn_sync(
        NULL,
        argv,
        NULL,
        G_SPAWN_SEARCH_PATH,
        NULL, NULL,
        &stdout_buf,
        &stderr_buf,
        &exit_status,
        &error
    );

    g_free(pid_str);

    struct label_update_data *upd = g_new(struct label_update_data, 1);
    upd->button = data->button;
    upd->label = GTK_LABEL(status_label);
    upd->new_label = g_strdup("Attach");

    if (error) {
        upd->status_msg = g_strdup_printf("Exec error: %s", error->message);
        g_error_free(error);
    } else if (!success || exit_status != 0) {
        gchar *err_msg = stderr_buf ? g_strdup(stderr_buf) : g_strdup("Unknown error");
        upd->status_msg = g_strdup_printf("Injection failed (code %d): %s", exit_status, err_msg);
        g_free(err_msg);
    } else {
        upd->status_msg = g_strdup("Injection successful!");
    }

    g_idle_add(update_button_and_status, upd);
    g_free(data);
    g_free(stdout_buf);
    g_free(stderr_buf);
    
    return NULL;
}

static void on_attach_clicked(GtkButton *button, gpointer user_data) {
    (void)user_data;
    
    gtk_button_set_label(button, "Attaching...");
    gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
    gtk_label_set_text(GTK_LABEL(status_label), "Attaching to Sober...");
    
    struct attach_data *data = (struct attach_data *)g_malloc(sizeof(struct attach_data));
    data->button = button;
    
    pthread_t thread;
    if (pthread_create(&thread, NULL, attach_thread_func, data) != 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "Failed to create thread");
        g_free(data);
    } else {
        pthread_detach(thread);
    }
}

static void activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    
    main_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(main_window), "Wineblox Executor - Linux");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 1000, 700);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_window_set_child(GTK_WINDOW(main_window), vbox);

    // Notebook for tabs
    notebook = gtk_notebook_new();
    gtk_widget_set_hexpand(notebook, TRUE);
    gtk_widget_set_vexpand(notebook, TRUE);
    gtk_box_append(GTK_BOX(vbox), notebook);

    // Editor Tab
    GtkWidget *editor_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    editor = gtk_text_view_new();
    gtk_widget_set_hexpand(editor, TRUE);
    gtk_widget_set_vexpand(editor, TRUE);
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), editor);
    gtk_box_append(GTK_BOX(editor_vbox), scroll);

    // Button box
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_set_homogeneous(GTK_BOX(button_box), FALSE);
    gtk_box_append(GTK_BOX(editor_vbox), button_box);

    GtkWidget *btn_execute = gtk_button_new_with_label("Execute");
    GtkWidget *btn_clear = gtk_button_new_with_label("Clear");
    GtkWidget *btn_open = gtk_button_new_with_label("Open File");
    GtkWidget *btn_save = gtk_button_new_with_label("Save File");
    attach_button = gtk_button_new_with_label("Attach");

    gtk_box_append(GTK_BOX(button_box), btn_execute);
    gtk_box_append(GTK_BOX(button_box), btn_clear);
    gtk_box_append(GTK_BOX(button_box), btn_open);
    gtk_box_append(GTK_BOX(button_box), btn_save);
    gtk_box_append(GTK_BOX(button_box), attach_button);

    g_signal_connect(btn_execute, "clicked", G_CALLBACK(on_execute_clicked), NULL);
    g_signal_connect(btn_clear, "clicked", G_CALLBACK(on_clear_clicked), NULL);
    g_signal_connect(btn_open, "clicked", G_CALLBACK(on_open_file_clicked), NULL);
    g_signal_connect(btn_save, "clicked", G_CALLBACK(on_save_file_clicked), NULL);
    g_signal_connect(attach_button, "clicked", G_CALLBACK(on_attach_clicked), NULL);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), editor_vbox, gtk_label_new("Editor"));

    // Script Hub Tab
    GtkWidget *hub_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_top(hub_vbox, 10);
    gtk_widget_set_margin_bottom(hub_vbox, 10);
    gtk_widget_set_margin_start(hub_vbox, 10);
    gtk_widget_set_margin_end(hub_vbox, 10);

    GtkWidget *category_label = gtk_label_new("Category:");
    GtkWidget *category_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(category_combo), "roblox", "Roblox");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(category_combo), "game", "Game");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(category_combo), "exploit", "Exploits");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(category_combo), "utility", "Utilities");
    gtk_combo_box_set_active(GTK_COMBO_BOX(category_combo), 0);

    GtkWidget *category_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(category_box), category_label);
    gtk_box_append(GTK_BOX(category_box), category_combo);
    gtk_box_append(GTK_BOX(hub_vbox), category_box);

    // Scripts list
    script_hub_view = gtk_list_box_new();
    gtk_widget_set_vexpand(script_hub_view, TRUE);
    gtk_widget_set_hexpand(script_hub_view, TRUE);
    GtkWidget *script_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(script_scroll), script_hub_view);
    gtk_box_append(GTK_BOX(hub_vbox), script_scroll);

    g_signal_connect(category_combo, "changed", G_CALLBACK(on_category_changed), script_hub_view);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), hub_vbox, gtk_label_new("Script Hub"));

    // Status label
    status_label = gtk_label_new("Ready");
    gtk_label_set_xalign(GTK_LABEL(status_label), 0.0);
    gtk_box_append(GTK_BOX(vbox), status_label);

    gtk_window_present(GTK_WINDOW(main_window));
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    app = gtk_application_new("com.wineblox.executor", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
