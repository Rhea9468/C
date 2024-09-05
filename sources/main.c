#include <gtk/gtk.h>
#include <mpg123.h>
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>

// Function prototypes
void on_open_file_menu_item_clicked(GtkWidget *widget, gpointer user_data);
void on_file_chooser_response(GObject *dialog, GAsyncResult *res, gpointer user_data);
void create_menu(GtkWidget *menu_bar);
void apply_css(GtkApplication *app);
void play_mp3(const char *filename);
void cleanup_audio();
void audio_callback(void *userdata, Uint8 *stream, int len);

// Global variables
static mpg123_handle *mpg123_dec = NULL;
static double volume = 0.04;
static bool is_paused = false;
static bool is_playing = false;

// Function implementations

void on_open_file_menu_item_clicked(GtkWidget *widget, gpointer user_data) {
    GtkFileDialog *dialog = gtk_file_dialog_new();

    // Open the file chooser dialog
    gtk_file_dialog_open(GTK_FILE_DIALOG(dialog), GTK_WINDOW(gtk_widget_get_native(widget)), NULL,
                         (GAsyncReadyCallback)on_file_chooser_response, NULL);
}

void on_file_chooser_response(GObject *dialog, GAsyncResult *res, gpointer user_data) {
    GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(dialog), res, NULL);
    if (file) {
        char *filename = g_file_get_path(file);
        g_print("Selected file: %s\n", filename);

        if (is_playing) {
            // Stop current playback
            SDL_PauseAudio(1);
            is_playing = false;
        }

        // Start playback with the selected file
        play_mp3(filename);
        g_free(filename);
        g_object_unref(file);
    }
}

void cleanup_audio() {
    if (mpg123_dec) {
        SDL_CloseAudio(); // Close audio device
        SDL_Quit();       // Quit SDL
        mpg123_delete(mpg123_dec);
        mpg123_exit();
        mpg123_dec = NULL;
    }
}

void play_mp3(const char *filename) {
    int err;

    cleanup_audio(); // Clean up any previous audio

    if (mpg123_init() != MPG123_OK) {
        fprintf(stderr, "mpg123 initialization failed\n");
        return;
    }

    mpg123_dec = mpg123_new(NULL, &err);
    if (!mpg123_dec) {
        fprintf(stderr, "mpg123_new() failed\n");
        mpg123_exit();
        return;
    }

    if (mpg123_open(mpg123_dec, filename) != MPG123_OK) {
        fprintf(stderr, "mpg123_open() failed: %s\n", mpg123_strerror(mpg123_dec));
        mpg123_delete(mpg123_dec);
        mpg123_exit();
        return;
    }

    SDL_AudioSpec spec;
    int channels, encoding;
    long rate;

    mpg123_getformat(mpg123_dec, &rate, &channels, &encoding);
    spec.freq = rate;
    spec.format = AUDIO_S16SYS;
    spec.channels = channels;
    spec.samples = 4096;
    spec.callback = audio_callback;
    spec.userdata = NULL;

    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init() failed: %s\n", SDL_GetError());
        mpg123_delete(mpg123_dec);
        mpg123_exit();
        return;
    }

    if (SDL_OpenAudio(&spec, NULL) != 0) {
        fprintf(stderr, "SDL_OpenAudio() failed: %s\n", SDL_GetError());
        SDL_Quit();
        mpg123_delete(mpg123_dec);
        mpg123_exit();
        return;
    }

    SDL_PauseAudio(0); // Start audio playback
    is_playing = true;
}

void audio_callback(void *userdata, Uint8 *stream, int len) {
    size_t done;
    if (mpg123_dec && is_playing) {
        mpg123_read(mpg123_dec, stream, len, &done);

        for (int i = 0; i < len; i += 2) {  // Assuming 16-bit samples
            Sint16 sample = ((Sint16*)stream)[i / 2];
            sample = (Sint16)(sample * volume);
            ((Sint16*)stream)[i / 2] = sample;
        }
    }
}


static void on_ok_button_clicked(GtkButton *button, gpointer user_data) {
    g_print("OK Button clicked\n");
}

static void on_cancel_button_clicked(GtkButton *button, gpointer user_data) {
    g_print("Cancel Button clicked\n");
}


void apply_css(GtkApplication *app) {
    GtkCssProvider *provider = gtk_css_provider_new();
    GError *error = NULL;
    gtk_css_provider_load_from_path(provider, "styles.css");
    
    if (error) {
        g_print("Error loading CSS file: %s\n", error->message);
        g_error_free(error);
    }

    GdkDisplay *display = gdk_display_get_default();
    gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    g_object_unref(provider); // Clean up CSS provider
}

void create_menu(GtkWidget *menu_bar) {
    GtkBuilder *builder = gtk_builder_new_from_file("menu.ui");

    GMenuModel *menu_model = G_MENU_MODEL(gtk_builder_get_object(builder, "app-menu"));
    GtkWidget *menu_button = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_button), "open-menu-symbolic");
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_button), menu_model);

    gtk_box_append(GTK_BOX(menu_bar), menu_button); // GTK4 uses gtk_box_append instead of gtk_box_pack_start
    g_object_unref(builder); // Clean up the builder
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *menu_bar;
    GtkWidget *action_bar;
    GtkWidget *ok_button;
    GtkWidget *cancel_button;

    // Create a new window
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "MP3 Player");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 720);

    // Apply custom CSS styles
    apply_css(app);

    // Create a vertical box container for the menu and action bar
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    // Create the menu bar (or similar top-level widget)
    menu_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    create_menu(menu_bar); // Add menu items to the menu_bar
    gtk_box_append(GTK_BOX(vbox), menu_bar); // Add the menu bar to the vbox

    // Add a placeholder or other widgets here if needed (like a media player UI)

    // Create the action bar
    action_bar = gtk_action_bar_new();
    gtk_box_append(GTK_BOX(vbox), action_bar); // Add action bar to the vbox

    // Create and add OK and Cancel buttons to the action bar
    ok_button = gtk_button_new_with_label("OK");
    g_signal_connect(ok_button, "clicked", G_CALLBACK(on_ok_button_clicked), NULL);
    gtk_action_bar_pack_end(GTK_ACTION_BAR(action_bar), ok_button);

    cancel_button = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(on_cancel_button_clicked), NULL);
    gtk_action_bar_pack_start(GTK_ACTION_BAR(action_bar), cancel_button);

    // Set the vertical box as the main child of the window
    gtk_window_set_child(GTK_WINDOW(window), vbox); 

    // Present the window (equivalent to gtk_widget_show_all in GTK3)
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char *argv[]) {
    GtkApplication *app;
    int status;

    app = gtk_application_new("com.example.Mp3Player", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
