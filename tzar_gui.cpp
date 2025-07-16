// === tzar_gui.cpp ===
#include <gtk/gtk.h> // Include the GTK+ 3 header
#include <iostream>  // For std::cout, std::cerr
#include <string>    // For std::string
#include <vector>    // For std::vector
#include <sstream>   // For std::ostringstream
#include <cstdlib>   // For std::system
#include <fstream>   // For file stream operations (ifstream)
#include <cstdint>   // For fixed-width integer types (uint32_t, uint64_t)
#include <filesystem> // For path manipulation

namespace fs = std::filesystem; // Alias for std::filesystem

// Global pointers to GTK widgets for easy access in callbacks
GtkEntry *output_name_entry; // Still used for "Create Archive" dialog
GtkTextView *log_text_view;
GtkTreeView *file_list_tree_view;
GtkListStore *file_list_store;
GtkStatusbar *status_bar; // New status bar

// Store the path of the currently opened archive
std::string current_archive_path;
bool current_archive_is_encrypted = false; // New: Flag to track encryption status

// Enum for columns in the GtkListStore
enum {
    COL_FILENAME = 0,
    COL_FILESIZE,
    NUM_COLS // Keep track of the number of columns
};

// Forward declarations for functions
GtkWidget* create_menu_item(const gchar* label, GCallback callback, gpointer data);
void push_status_message(const std::string& message);
void append_to_log(const std::string& text);
std::string gui_readString(std::ifstream& inFile);
uint64_t gui_readBinaryDataSizeAndSkip(std::ifstream& inFile);
void load_archive_contents(const std::string& archive_path);
std::string get_password_from_dialog(GtkWindow* parent_window, const std::string& title);


// Function to push a message to the status bar
void push_status_message(const std::string& message) {
    guint context_id = gtk_statusbar_get_context_id(status_bar, "general");
    gtk_statusbar_push(status_bar, context_id, message.c_str());
}

// Function to append text to the log TextView
void append_to_log(const std::string& text) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(log_text_view);
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_buffer_insert(buffer, &iter, text.c_str(), -1);
    // Scroll to the end
    gtk_text_view_scroll_to_mark(log_text_view, gtk_text_buffer_get_insert(buffer), 0.0, TRUE, 0.0, 0.0);
}

// Helper function to read a string from an input file stream (for archive parsing)
std::string gui_readString(std::ifstream& inFile) {
    uint32_t len;
    inFile.read(reinterpret_cast<char*>(&len), sizeof(len));
    if (!inFile) {
        throw std::runtime_error("Error reading string length from archive.");
    }
    std::vector<char> buffer(len);
    inFile.read(buffer.data(), len);
    if (!inFile) {
        throw std::runtime_error("Error reading string data from archive.");
    }
    return std::string(buffer.begin(), buffer.end());
}

// Helper function to read binary data size from an input file stream (for archive parsing)
uint64_t gui_readBinaryDataSizeAndSkip(std::ifstream& inFile) {
    uint64_t size;
    inFile.read(reinterpret_cast<char*>(&size), sizeof(size));
    if (!inFile) {
        throw std::runtime_error("Error reading binary data size from archive.");
    }
    inFile.seekg(size, std::ios_base::cur);
    if (!inFile) {
        throw std::runtime_error("Error skipping binary data content in archive.");
    }
    return size;
}

// Function to load and display archive contents
void load_archive_contents(const std::string& archive_path) {
    append_to_log("Viewing contents of: " + archive_path + "\n");
    gtk_list_store_clear(file_list_store); // Clear previous contents
    current_archive_is_encrypted = false; // Reset encryption status

    std::ifstream archiveFile(archive_path, std::ios::binary);
    if (!archiveFile.is_open()) {
        append_to_log("Error: Could not open archive file for viewing: " + archive_path + "\n");
        push_status_message("Error: Could not open archive.");
        return;
    }

    // Read the encryption flag (first byte)
    uint8_t encryption_flag = archiveFile.get();
    if (archiveFile.eof()) {
        append_to_log("Error: Archive is empty or corrupted (missing encryption flag).\n");
        push_status_message("Error: Empty or corrupted archive.");
        archiveFile.close();
        return;
    }

    if (encryption_flag == 0x01) {
        current_archive_is_encrypted = true;
        append_to_log("Archive detected as encrypted (.tzar2 format).\n");
        push_status_message("Encrypted archive loaded.");
    } else if (encryption_flag == 0x00) {
        current_archive_is_encrypted = false;
        append_to_log("Archive detected as unencrypted (.tzar format).\n");
        push_status_message("Unencrypted archive loaded.");
    } else {
        append_to_log("Warning: Unknown archive format flag (0x" + std::to_string(encryption_flag) + "). Assuming unencrypted.\n");
        push_status_message("Warning: Unknown archive format.");
        // Seek back to beginning to re-read if it's an old .tzar without flag, or just malformed
        archiveFile.seekg(0, std::ios::beg);
    }

    try {
        // Skip the flag byte if it was read and valid, otherwise seek to beginning for old format
        if (encryption_flag == 0x00 || encryption_flag == 0x01) {
            // Already read the flag, so we are at the correct position to read metadata
        } else {
            // For unknown flag, assume it's an old .tzar file that doesn't have the flag.
            // Seek to the beginning to parse it as a standard .tzar.
            archiveFile.seekg(0, std::ios::beg);
        }

        while (archiveFile.peek() != EOF) {
            std::string filePath = gui_readString(archiveFile);
            uint64_t fileSize = gui_readBinaryDataSizeAndSkip(archiveFile);

            GtkTreeIter iter;
            gtk_list_store_append(file_list_store, &iter);
            gtk_list_store_set(file_list_store, &iter,
                               COL_FILENAME, filePath.c_str(),
                               COL_FILESIZE, (gint64)fileSize,
                               -1);
        }
        append_to_log("Contents metadata parsed successfully.\n");
        current_archive_path = archive_path; // Store the path of the successfully opened archive
    } catch (const std::runtime_error& e) {
        append_to_log("Error parsing archive metadata: " + std::string(e.what()) + "\n");
        push_status_message("Error parsing archive metadata.");
    } catch (const std::exception& e) {
        append_to_log("An unexpected error occurred while parsing archive metadata: " + std::string(e.what()) + "\n");
        push_status_message("Unexpected error during parsing.");
    }

    archiveFile.close();
}

// Helper function to get password from a dialog
std::string get_password_from_dialog(GtkWindow* parent_window, const std::string& title) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons(title.c_str(),
                                                     parent_window,
                                                     (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                     "_Cancel", GTK_RESPONSE_CANCEL,
                                                     "_OK", GTK_RESPONSE_OK,
                                                     NULL);
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *label = gtk_label_new("Enter password:");
    GtkEntry *password_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_visibility(password_entry, FALSE); // Mask password input
    gtk_entry_set_invisible_char(password_entry, '*'); // Character to display for masked input

    gtk_container_add(GTK_CONTAINER(content_area), label);
    gtk_container_add(GTK_CONTAINER(content_area), GTK_WIDGET(password_entry));
    gtk_widget_show_all(dialog);

    std::string password;
    gint res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_OK) {
        password = gtk_entry_get_text(password_entry);
    }
    gtk_widget_destroy(dialog);
    return password;
}


// Callback for "File -> Open Archive" menu item
static void on_open_archive_menu_item_activated(GtkMenuItem *menuitem, gpointer user_data) {
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;

    dialog = gtk_file_chooser_dialog_new("Open .tzar Archive",
                                         GTK_WINDOW(user_data),
                                         action,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_Open", GTK_RESPONSE_ACCEPT,
                                         NULL);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "tZAR Archives (*.tzar, *.tzar2)");
    gtk_file_filter_add_pattern(filter, "*.tzar");
    gtk_file_filter_add_pattern(filter, "*.tzar2");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        char *filename = gtk_file_chooser_get_filename(chooser);
        load_archive_contents(filename);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

// Callback for "File -> Create Archive" menu item
static void on_create_archive_menu_item_activated(GtkMenuItem *menuitem, gpointer user_data) {
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN; // Allows selecting files and folders
    gint res;

    dialog = gtk_file_chooser_dialog_new("Select Files/Folders to Archive",
                                         GTK_WINDOW(user_data),
                                         action,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_Create", GTK_RESPONSE_ACCEPT,
                                         NULL);

    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(chooser);
        GSList *files = gtk_file_chooser_get_filenames(chooser);

        // Prompt for output archive base name
        GtkWidget *name_dialog = gtk_dialog_new_with_buttons("Enter Archive Name",
                                                             GTK_WINDOW(user_data),
                                                             (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT), // Explicit cast
                                                             "_Cancel", GTK_RESPONSE_CANCEL,
                                                             "_OK", GTK_RESPONSE_OK,
                                                             NULL);
        GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(name_dialog));
        GtkWidget *label = gtk_label_new("Enter the base name for the new .tzar archive:");
        output_name_entry = GTK_ENTRY(gtk_entry_new()); // Re-use global entry for simplicity, or create new local
        gtk_entry_set_text(output_name_entry, "new_archive"); // Default name
        gtk_container_add(GTK_CONTAINER(content_area), label);
        gtk_container_add(GTK_CONTAINER(content_area), GTK_WIDGET(output_name_entry));
        gtk_widget_show_all(name_dialog);

        gint name_res = gtk_dialog_run(GTK_DIALOG(name_dialog));
        std::string output_base_name;
        if (name_res == GTK_RESPONSE_OK) {
            output_base_name = gtk_entry_get_text(output_name_entry);
        }
        gtk_widget_destroy(name_dialog); // Destroy the name input dialog

        if (output_base_name.empty()) {
            append_to_log("Error: Output archive name cannot be empty. Archiving cancelled.\n");
            push_status_message("Archiving cancelled: No output name.");
            g_slist_free_full(files, g_free); // Free filenames if name dialog was cancelled
            gtk_widget_destroy(dialog);
            return;
        }

        std::ostringstream command_stream;
        command_stream << "./simple_archiver \"" << output_base_name << "\"";

        for (GSList *l = files; l != NULL; l = l->next) {
            command_stream << " \"" << (const char*)l->data << "\"";
            g_free(l->data);
        }
        g_slist_free(files);

        std::string command = command_stream.str();
        append_to_log("Executing: " + command + "\n");
        push_status_message("Creating archive...");

        int result = std::system(command.c_str());

        if (result == 0) {
            append_to_log("Archiving process completed successfully.\n");
            push_status_message("Archive created successfully.");
            // Optionally, load the new archive's contents after creation
            load_archive_contents(output_base_name + ".tzar"); 
        } else {
            append_to_log("Archiving process failed with exit code: " + std::to_string(result) + "\n");
            push_status_message("Archiving failed.");
        }
    }

    gtk_widget_destroy(dialog);
}

// Callback for "File -> Encrypt Archive" menu item
static void on_encrypt_archive_menu_item_activated(GtkMenuItem *menuitem, gpointer user_data) {
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;

    dialog = gtk_file_chooser_dialog_new("Select .tzar Archive to Encrypt",
                                         GTK_WINDOW(user_data),
                                         action,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_Encrypt", GTK_RESPONSE_ACCEPT,
                                         NULL);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "tZAR Archives (*.tzar)");
    gtk_file_filter_add_pattern(filter, "*.tzar");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        char *input_filename = gtk_file_chooser_get_filename(chooser);
        
        std::string password = get_password_from_dialog(GTK_WINDOW(user_data), "Enter Encryption Password");
        if (password.empty()) {
            append_to_log("Encryption cancelled: No password entered.\n");
            push_status_message("Encryption cancelled.");
            g_free(input_filename);
            gtk_widget_destroy(dialog);
            return;
        }

        fs::path input_path_fs(input_filename);
        std::string output_base_name = input_path_fs.stem().string(); // Use input filename's base name for output

        std::ostringstream command_stream;
        command_stream << "./tzar_encrypt \"" << input_filename << "\" \"" << output_base_name << "\" \"" << password << "\"";
        
        std::string command = command_stream.str();
        append_to_log("Executing: " + command + "\n");
        push_status_message("Encrypting archive...");

        int result = std::system(command.c_str());

        if (result == 0) {
            append_to_log("Encryption process completed successfully.\n");
            push_status_message("Archive encrypted successfully.");
            // Optionally, load the new encrypted archive's contents
            load_archive_contents(output_base_name + ".tzar2"); 
        } else {
            append_to_log("Encryption process failed with exit code: " + std::to_string(result) + "\n");
            push_status_message("Encryption failed.");
        }
        g_free(input_filename);
    }
    gtk_widget_destroy(dialog);
}

// Callback for "File -> Decrypt Archive" menu item
static void on_decrypt_archive_menu_item_activated(GtkMenuItem *menuitem, gpointer user_data) {
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;

    dialog = gtk_file_chooser_dialog_new("Select .tzar2 Archive to Decrypt",
                                         GTK_WINDOW(user_data),
                                         action,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_Decrypt", GTK_RESPONSE_ACCEPT,
                                         NULL);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "tZAR Encrypted Archives (*.tzar2)");
    gtk_file_filter_add_pattern(filter, "*.tzar2");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        char *input_filename = gtk_file_chooser_get_filename(chooser);
        
        std::string password = get_password_from_dialog(GTK_WINDOW(user_data), "Enter Decryption Password");
        if (password.empty()) {
            append_to_log("Decryption cancelled: No password entered.\n");
            push_status_message("Decryption cancelled.");
            g_free(input_filename);
            gtk_widget_destroy(dialog);
            return;
        }

        std::ostringstream command_stream;
        command_stream << "./tzar_decrypt \"" << input_filename << "\" \"" << password << "\"";
        
        std::string command = command_stream.str();
        append_to_log("Executing: " + command + "\n");
        push_status_message("Decrypting archive...");

        int result = std::system(command.c_str());

        if (result == 0) {
            append_to_log("Decryption process completed successfully.\n");
            push_status_message("Archive decrypted successfully.");
            // Optionally, load the original unencrypted archive's contents after decryption
            // Note: tzar_decrypt creates a folder named after the archive.
            // We can't easily load the contents of that folder into the tree view
            // without more complex logic or re-archiving it.
            // For now, just confirm decryption.
        } else {
            append_to_log("Decryption process failed with exit code: " + std::to_string(result) + "\n");
            push_status_message("Decryption failed.");
        }
        g_free(input_filename);
    }
    gtk_widget_destroy(dialog);
}


// Callback for "File -> Extract All" menu item
static void on_extract_all_menu_item_activated(GtkMenuItem *menuitem, gpointer user_data) {
    if (current_archive_path.empty()) {
        append_to_log("Error: No archive is currently open to extract.\n");
        push_status_message("No archive open for extraction.");
        return;
    }

    std::ostringstream command_stream;

    if (current_archive_is_encrypted) {
        std::string password = get_password_from_dialog(GTK_WINDOW(user_data), "Enter Decryption Password");
        if (password.empty()) {
            append_to_log("Extraction cancelled: No password entered.\n");
            push_status_message("Extraction cancelled.");
            return;
        }
        command_stream << "./tzar_decrypt \"" << current_archive_path << "\" \"" << password << "\"";
    } else {
        command_stream << "./simple_unarchiver \"" << current_archive_path << "\"";
    }

    std::string command = command_stream.str();
    append_to_log("Executing: " + command + "\n");
    push_status_message("Extracting all contents...");

    int result = std::system(command.c_str());

    if (result == 0) {
        append_to_log("Unarchiving process completed successfully.\n");
        push_status_message("Extraction complete.");
    } else {
        append_to_log("Unarchiving process failed with exit code: " + std::to_string(result) + "\n");
        push_status_message("Extraction failed.");
    }
}

// Callback for "Extract Selected" context menu item
static void on_extract_selected_menu_item_activated(GtkMenuItem *menuitem, gpointer user_data) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(file_list_tree_view);
    GtkTreeModel *model;
    GList *rows = gtk_tree_selection_get_selected_rows(selection, &model); 

    if (rows == NULL) {
        append_to_log("No file(s) selected for extraction.\n");
        push_status_message("No file(s) selected.");
        return;
    }

    if (current_archive_path.empty()) {
        append_to_log("Error: No archive is currently open to extract from.\n");
        push_status_message("No archive open for extraction.");
        g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free); 
        return;
    }

    if (current_archive_is_encrypted) {
        append_to_log("Selective extraction from encrypted archives (.tzar2) is not yet supported.\n");
        push_status_message("Selective extract (encrypted) not supported.");
        g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free); 
        return;
    }

    std::ostringstream command_stream;
    command_stream << "./simple_unarchiver \"" << current_archive_path << "\"";

    // Iterate through selected rows and append each filename to the command
    for (GList *l = rows; l != NULL; l = g_list_next(l)) { 
        GtkTreePath *path = (GtkTreePath *)l->data;
        GtkTreeIter iter;
        if (gtk_tree_model_get_iter(model, &iter, path)) {
            gchar *filename_gstr;
            gtk_tree_model_get(model, &iter, COL_FILENAME, &filename_gstr, -1);
            command_stream << " \"" << filename_gstr << "\"";
            g_free(filename_gstr);
        }
    }
    g_list_free_full(rows, (GDestroyNotify)gtk_tree_path_free); 

    std::string command = command_stream.str();
    append_to_log("Executing: " + command + "\n");
    push_status_message("Extracting selected item(s)...");

    int result = std::system(command.c_str());

    if (result == 0) {
        append_to_log("Selected file(s) extracted successfully.\n");
        push_status_message("Selected item(s) extracted.");
    } else {
        append_to_log("Failed to extract selected file(s).\n");
        push_status_message("Selected item(s) extraction failed.");
    }
}

// Callback for right-click on the GtkTreeView
static gboolean on_tree_view_button_press_event(GtkWidget *tree_view, GdkEventButton *event, gpointer user_data) {
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) { // Right-click
        GtkTreePath *path;
        GtkTreeViewColumn *column;
        gint cell_x, cell_y;

        // Get the clicked row
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tree_view), (gint)event->x, (gint)event->y,
                                          &path, &column, &cell_x, &cell_y)) {
            // A row was clicked, show the context menu
            GtkWidget *menu = gtk_menu_new();
            GtkWidget *extract_selected_item = create_menu_item("Extract Selected...", G_CALLBACK(on_extract_selected_menu_item_activated), NULL);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), extract_selected_item);
            
            gtk_widget_show_all(menu);
            gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)event); // Show menu at mouse position
            
            gtk_tree_path_free(path); // Free the path
            return TRUE; // Event handled
        }
    }
    return FALSE; // Event not handled
}

// Function to set up the columns for the GtkTreeView
static void setup_tree_view_columns(GtkTreeView *tree_view) {
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("File Name", renderer, "text", COL_FILENAME, NULL);
    gtk_tree_view_append_column(tree_view, column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Size (bytes)", renderer, "text", COL_FILESIZE, NULL);
    gtk_tree_view_append_column(tree_view, column);
}

// Function to create a menu item
GtkWidget* create_menu_item(const gchar* label, GCallback callback, gpointer data) {
    GtkWidget *menu_item = gtk_menu_item_new_with_label(label);
    g_signal_connect(menu_item, "activate", callback, data);
    return menu_item;
}

// Main function for the GTK application
int main(int argc, char *argv[]) {
    GtkWidget *window;
    GtkWidget *vbox; // Use a vertical box for main layout
    GtkWidget *menubar;
    GtkWidget *file_menu_item;
    GtkWidget *file_menu;
    GtkWidget *open_archive_item;
    GtkWidget *create_archive_item;
    GtkWidget *encrypt_archive_item; // New menu item
    GtkWidget *decrypt_archive_item; // New menu item
    GtkWidget *extract_all_item;
    GtkWidget *separator_item1; // Separator for file operations
    GtkWidget *separator_item2; // Separator for quit
    GtkWidget *separator_item3; // New: Separator for extract options before quit
    GtkWidget *quit_item;
    GtkWidget *file_list_scrolled_window;
    GtkWidget *log_scrolled_window;

    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "tZAR Archiver/Unarchiver");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Main vertical box to hold menu bar, content, and status bar
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Create a menu bar
    menubar = gtk_menu_bar_new();
    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0); // Pack at the top

    // Create "File" menu
    file_menu_item = gtk_menu_item_new_with_label("File");
    file_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_menu_item), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file_menu_item);

    // Add items to "File" menu
    open_archive_item = create_menu_item("Open Archive...", G_CALLBACK(on_open_archive_menu_item_activated), window);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), open_archive_item);

    create_archive_item = create_menu_item("Create Archive...", G_CALLBACK(on_create_archive_menu_item_activated), window);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), create_archive_item);

    separator_item1 = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), separator_item1);

    encrypt_archive_item = create_menu_item("Encrypt Archive...", G_CALLBACK(on_encrypt_archive_menu_item_activated), window);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), encrypt_archive_item);

    decrypt_archive_item = create_menu_item("Decrypt Archive...", G_CALLBACK(on_decrypt_archive_menu_item_activated), window);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), decrypt_archive_item);

    separator_item2 = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), separator_item2);

    extract_all_item = create_menu_item("Extract All...", G_CALLBACK(on_extract_all_menu_item_activated), window);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), extract_all_item);

    // Corrected: Use separator_item3 for the last separator before Quit
    separator_item3 = gtk_separator_menu_item_new(); 
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), separator_item3);

    quit_item = create_menu_item("Quit", G_CALLBACK(gtk_main_quit), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), quit_item);

    // File List Tree View (main content area)
    file_list_store = gtk_list_store_new(NUM_COLS, G_TYPE_STRING, G_TYPE_INT64);
    file_list_tree_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(file_list_store)));
    gtk_tree_view_set_headers_visible(file_list_tree_view, TRUE);
    setup_tree_view_columns(file_list_tree_view);

    // Allow selecting multiple rows
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(file_list_tree_view), GTK_SELECTION_MULTIPLE);

    // Connect the button-press-event to handle right-clicks
    g_signal_connect(file_list_tree_view, "button-press-event", G_CALLBACK(on_tree_view_button_press_event), NULL);


    file_list_scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(file_list_scrolled_window), GTK_WIDGET(file_list_tree_view));
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_list_scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    // Pack the tree view to expand and fill available space
    gtk_box_pack_start(GTK_BOX(vbox), file_list_scrolled_window, TRUE, TRUE, 0);

    // Log Text View (for displaying command output/messages)
    log_text_view = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(log_text_view, FALSE);
    gtk_text_view_set_cursor_visible(log_text_view, FALSE);
    gtk_text_view_set_monospace(log_text_view, TRUE); // Use monospace font for logs

    log_scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(log_scrolled_window), GTK_WIDGET(log_text_view));
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(log_scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(log_scrolled_window, -1, 100); // Give log a fixed height
    gtk_box_pack_start(GTK_BOX(vbox), log_scrolled_window, FALSE, FALSE, 0);

    // Status Bar
    status_bar = GTK_STATUSBAR(gtk_statusbar_new());
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(status_bar), FALSE, FALSE, 0); // Pack at the bottom

    gtk_widget_show_all(window);
    gtk_main();

    g_object_unref(file_list_store);

    return 0;
}
