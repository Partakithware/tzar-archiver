tZAR Archiver/Unarchiver

A custom archiving system developed in C++ with a GTK+ 3 graphical user interface (GUI) for creating, extracting, and password-protecting archives.
Features

    Archive Creation (.tzar): Compress multiple files and directories into a single .tzar archive.

    Archive Extraction (.tzar): Extract all contents or selectively extract specific files/directories from a .tzar archive.

    Password-Based Encryption (.tzar2): Encrypt existing .tzar archives with a password, creating a .tzar2 file. This uses a basic XOR cipher with a SHA256-derived key.

    Password-Based Decryption (.tzar2): Decrypt .tzar2 archives using the correct password to restore original contents.

    Graphical User Interface (GUI): A GTK+ 3 based interface for intuitive interaction with the archiving and encryption functionalities.

        Open and view contents of .tzar and .tzar2 archives.

        Create new archives.

        Extract all contents from an opened archive.

        Right-click context menu for selective extraction of files from unencrypted archives.

        Prompts for passwords when interacting with encrypted archives.

Project Structure

The project consists of several C++ executables and a GUI application:

    simple_archiver.cpp: Command-line tool for creating .tzar archives.

    simple_unarchiver.cpp: Command-line tool for extracting .tzar archive contents (all or selected).

    tzar_encrypt.cpp: Command-line tool for encrypting .tzar archives into .tzar2 archives.

    tzar_decrypt.cpp: Command-line tool for decrypting .tzar2 archives.

    tzar_gui.cpp: The GTK+ 3 based graphical interface that orchestrates the command-line tools.

File Formats
.tzar (Unencrypted Archive)

A simple, sequential binary format:

Field
	

Type
	

Description

Encryption Flag
	

uint8_t
	

0x00 (indicates unencrypted)

Filename Length
	

uint32_t
	

Length of the relative filename string.

Filename
	

char[]
	

Relative path and name of the archived item.

Content Size
	

uint64_t
	

Size of the item's binary content in bytes.

Content
	

char[]
	

Raw binary data of the item. (Empty for directories)

This structure repeats for each archived file or directory.
.tzar2 (Encrypted Archive)

An extension of the .tzar format, with encryption applied to the content:

Field
	

Type
	

Description

Encryption Flag
	

uint8_t
	

0x01 (indicates encrypted)

Filename Length
	

uint32_t
	

Length of the relative filename string.

Filename
	

char[]
	

Relative path and name of the archived item.

Encrypted Content Size
	

uint64_t
	

Size of the item's encrypted binary content.

Encrypted Content
	

char[]
	

XOR-encrypted binary data of the item.

This structure repeats for each archived file or directory.

Note on Encryption Security: The encryption implemented in tzar_encrypt.cpp and tzar_decrypt.cpp uses a basic XOR cipher with a SHA256-derived key. While this demonstrates the concept of password-based encryption, it is not cryptographically secure for protecting sensitive data against a determined attacker. This implementation is primarily for educational and conceptual purposes.
Building the Project
Prerequisites

    C++17 compatible compiler (e.g., g++)

    GTK+ 3 development libraries (for tzar_gui.cpp)

On Ubuntu/Debian:

sudo apt update
sudo apt install build-essential libgtk-3-dev

Compilation Steps

Navigate to the root directory of the project in your terminal.

    Compile Command-Line Tools:

    g++ simple_archiver.cpp -o simple_archiver -std=c++17
    g++ simple_unarchiver.cpp -o simple_unarchiver -std=c++17
    g++ tzar_encrypt.cpp -o tzar_encrypt -std=c++17
    g++ tzar_decrypt.cpp -o tzar_decrypt -std=c++17

    Compile GUI Application:

    g++ tzar_gui.cpp -o tzar_gui `pkg-config --cflags --libs gtk+-3.0` -std=c++17

Usage

Ensure all compiled executables (simple_archiver, simple_unarchiver, tzar_encrypt, tzar_decrypt, tzar_gui) are in the same directory or in your system's PATH.
GUI Usage

To launch the graphical interface:

./tzar_gui

Use the "File" menu options to:

    Open Archive...: Select a .tzar or .tzar2 file to view its contents.

    Create Archive...: Select files/folders to bundle into a new .tzar archive.

    Encrypt Archive...: Select an existing .tzar file to encrypt it into a password-protected .tzar2 file.

    Decrypt Archive...: Select an existing .tzar2 file to decrypt its contents into a new folder.

    Extract All...: Extract all contents of the currently opened archive. If it's a .tzar2 file, you will be prompted for a password.

    Extract Selected...: Right-click on a file in the list (only available for unencrypted .tzar files) to extract only that specific item.

Command-Line Tool Usage

These tools are primarily used by the GUI, but can also be invoked directly.
simple_archiver

Archives specified files and directories into a .tzar file.

./simple_archiver <output_archive_base_name> <input_file_or_directory1> [input_file_or_directory2 ...]

Example:

./simple_archiver my_archive_name my_document.txt my_folder/ another_file.jpg
# Creates my_archive_name.tzar

simple_unarchiver

Extracts contents from a .tzar archive.

./simple_unarchiver <input_archive_name.tzar> [file_to_extract1] [file_to_extract2 ...]

Examples:

    Extract all:

    ./simple_unarchiver my_archive_name.tzar

    Extract specific files:

    ./simple_unarchiver my_archive_name.tzar "my_folder/important.doc" "another_file.jpg"

tzar_encrypt

Encrypts an existing .tzar archive into a .tzar2 archive.

./tzar_encrypt <input_tzar_file> <output_base_name> [password]

Examples:

    Encrypt with password prompt:

    ./tzar_encrypt my_archive_name.tzar encrypted_archive
    # Prompts for password, creates encrypted_archive.tzar2

    Encrypt with password on command line (less secure for real passwords):

    ./tzar_encrypt my_archive_name.tzar encrypted_archive "MySecretPassword123"
    # Creates encrypted_archive.tzar2

tzar_decrypt

Decrypts a .tzar2 archive into a new directory.

./tzar_decrypt <input_tzar2_file> [password]

Examples:

    Decrypt with password prompt:

    ./tzar_decrypt encrypted_archive.tzar2
    # Prompts for password, extracts contents to 'encrypted_archive/' folder

    Decrypt with password on command line (less secure for real passwords):

    ./tzar_decrypt encrypted_archive.tzar2 "MySecretPassword123"
    # Extracts contents to 'encrypted_archive/' folder

Contributing

Feel free to fork the repository, open issues, or submit pull requests.
