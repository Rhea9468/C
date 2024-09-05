#include <gtk/gtk.h>
#include <mpg123.h>
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>

// Function prototypes
void on_open_file_menu_item_clicked(GtkWidget *widget, gpointer user_data);
void on_file_chooser_response(GtkWidget *dialog, int response_id, gpointer user_data);
void create_menu(GtkWidget *menu_bar);
static void on_pause_button_clicked(GtkWidget *widget, gpointer data);
static void on_stop_button_clicked(GtkWidget *widget, gpointer data);
static void on_close_button_clicked(GtkWidget *widget, gpointer data);
void play_mp3(const char *filename);
void cleanup_audio();
void audio_callback(void *userdata, Uint8 *stream, int len);  // Declare the audio callback

// Global variables
static mpg123_handle *mpg123_dec = NULL;
static double volume = 0.04;
static bool is_paused = false;
static bool is_playing = false;

// Function implementations

void on_open_file_menu_item_clicked(GtkWidget *widget, gpointer user_data) {
    GtkWidget *dialog;
    
    dialog = gtk_file_chooser_dialog_new("Open File",
                                         GTK_WINDOW(gtk_widget_get_toplevel(widget)),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         ("_Cancel"), GTK_RESPONSE_CANCEL,
                                         ("_Open"), GTK_RESPONSE_ACCEPT,
                                         NULL);
    
    g_signal_connect(dialog, "response", G_CALLBACK(on_file_chooser_response), NULL);
    gtk_widget_show_all(dialog);
}

void on_file_chooser_response(GtkWidget *dialog, int response_id, gpointer user_data) {
    if (response_id == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        char *filename = gtk_file_chooser_get_filename(chooser);

        // Print the file path
        printf("Selected file: %s\n", filename);

        // Stop any ongoing playback
        if (is_playing) {
            on_stop_button_clicked(NULL, NULL);
        }

        // Start playback with the selected file
        play_mp3(filename);

        g_free(filename);
    }
    
    gtk_widget_destroy(dialog);
}

void create_menu(GtkWidget *menu_bar) {
    GtkWidget *file_menu;
    GtkWidget *file_menu_item;
    GtkWidget *open_file_item;

    // Create the "File" menu
    file_menu = gtk_menu_new();
    file_menu_item = gtk_menu_item_new_with_label("File");
    gtk_widget_set_size_request(file_menu_item, 200, -1); // Set minimum width

    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_menu_item), file_menu);
    gtk_widget_set_size_request(file_menu, 200, -1); // Set minimum width

    // Create the "Open File" menu item
    open_file_item = gtk_menu_item_new_with_label("Open File");
    gtk_widget_set_size_request(open_file_item, 200, -1); // Set minimum width
    g_signal_connect(open_file_item, "activate", G_CALLBACK(on_open_file_menu_item_clicked), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), open_file_item);

    // Add the "File" menu item to the menu bar
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), file_menu_item);

    // Apply CSS class to menu items
    gtk_style_context_add_class(gtk_widget_get_style_context(file_menu_item), "menu-item");
    gtk_style_context_add_class(gtk_widget_get_style_context(open_file_item), "menu-item");
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

    cleanup_audio(); // Ensure any existing audio is cleaned up

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

static void on_pause_button_clicked(GtkWidget *widget, gpointer data) {
    if (is_paused) {
        SDL_PauseAudio(0);  // Resume audio
        gtk_button_set_label(GTK_BUTTON(widget), "Pause");
    } else {
        SDL_PauseAudio(1);  // Pause audio
        gtk_button_set_label(GTK_BUTTON(widget), "Resume");
    }
    is_paused = !is_paused;
}

static void on_stop_button_clicked(GtkWidget *widget, gpointer data) {
    if (is_playing) {
        is_playing = false;
        SDL_PauseAudio(1);  // Stop audio playback

        // Wait briefly to allow the audio callback to process
        SDL_Delay(100);

        cleanup_audio(); // Clean up audio resources
    }
}

static void on_close_button_clicked(GtkWidget *widget, gpointer data) {
    if (is_playing) {
        on_stop_button_clicked(NULL, NULL); // Ensure we stop the audio first
    }
    gtk_main_quit();  // Close the window
}

int main(int argc, char *argv[]) {
    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *menu_bar;
    GtkWidget *button_box;
    GtkWidget *bottom_box;
    GtkWidget *pause_button;
    GtkWidget *stop_button;
    GtkWidget *close_button;
    GtkWidget *spacer;
    GtkWidget *menu_box;

    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "MP3 Player");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 720);

    // Load the CSS file
    GtkCssProvider *provider = gtk_css_provider_new();
    GError *error = NULL;
    gtk_css_provider_load_from_path(provider, "../styles/main_menu.css", &error);
    if (error) {
        g_print("Error loading CSS file: %s\n", error->message);
        g_error_free(error);
    }

    GdkScreen *screen = gdk_screen_get_default();
    gtk_style_context_add_provider_for_screen(screen,
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // Create a vertical box to hold the menu bar and buttons
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    // Create and add the menu bar
    menu_bar = gtk_menu_bar_new();
    create_menu(menu_bar);

    // Create a box to center the menu bar
    menu_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_set_homogeneous(GTK_BOX(menu_box), TRUE); // Center the menu bar

    // Add the menu bar to the box
    gtk_box_pack_start(GTK_BOX(menu_box), menu_bar, FALSE, FALSE, 0);

    // Add the box to the vertical box
    gtk_box_pack_start(GTK_BOX(vbox), menu_box, FALSE, FALSE, 0);

    // Create a horizontal box for the top buttons
    button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    pause_button = gtk_button_new_with_label("Pause");
    g_signal_connect(pause_button, "clicked", G_CALLBACK(on_pause_button_clicked), NULL);

    stop_button = gtk_button_new_with_label("Stop");
    g_signal_connect(stop_button, "clicked", G_CALLBACK(on_stop_button_clicked), NULL);

    gtk_widget_set_size_request(pause_button, 100, 30); // Width: 200px, Height: 60px
    gtk_widget_set_size_request(stop_button, 100, 30);  // Width: 200px, Height: 60px

    gtk_box_pack_start(GTK_BOX(button_box), pause_button, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(button_box), stop_button, TRUE, TRUE, 5);

    gtk_box_pack_start(GTK_BOX(vbox), button_box, FALSE, FALSE, 0);

    // Create and add the bottom box with the close button
    bottom_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    close_button = gtk_button_new_with_label("Close");
    g_signal_connect(close_button, "clicked", G_CALLBACK(on_close_button_clicked), NULL);

    gtk_widget_set_size_request(close_button, 100, 30); // Width: 200px, Height: 60px

    gtk_box_pack_end(GTK_BOX(bottom_box), close_button, FALSE, FALSE, 5);

    gtk_box_pack_end(GTK_BOX(vbox), bottom_box, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(window), vbox);

    g_signal_connect(window, "destroy", G_CALLBACK(on_close_button_clicked), NULL);

    gtk_widget_show_all(window);

    gtk_main();

    return 0;
}
