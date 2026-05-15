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

static GtkWidget *main_window;
static GtkWidget *editor;
static GtkWidget *status_label;
static GtkWidget *attach_button;

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
    gtk_window_set_title(GTK_WINDOW(main_window), "Diesel Executor - Linux");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 900, 600);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_window_set_child(GTK_WINDOW(main_window), vbox);

    // Editor
    editor = gtk_text_view_new();
    gtk_widget_set_hexpand(editor, TRUE);
    gtk_widget_set_vexpand(editor, TRUE);
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), editor);
    gtk_box_append(GTK_BOX(vbox), scroll);

    // Button box
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_set_homogeneous(GTK_BOX(button_box), FALSE);
    gtk_box_append(GTK_BOX(vbox), button_box);

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

    // Status label
    status_label = gtk_label_new("Ready");
    gtk_label_set_xalign(GTK_LABEL(status_label), 0.0);
    gtk_box_append(GTK_BOX(vbox), status_label);

    g_signal_connect(btn_execute, "clicked", G_CALLBACK(on_execute_clicked), NULL);
    g_signal_connect(btn_clear, "clicked", G_CALLBACK(on_clear_clicked), NULL);
    g_signal_connect(btn_open, "clicked", G_CALLBACK(on_open_file_clicked), NULL);
    g_signal_connect(btn_save, "clicked", G_CALLBACK(on_save_file_clicked), NULL);
    g_signal_connect(attach_button, "clicked", G_CALLBACK(on_attach_clicked), NULL);

    gtk_window_present(GTK_WINDOW(main_window));
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    app = gtk_application_new("com.diesel.executor", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
