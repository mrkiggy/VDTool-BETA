#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <conio.h> // Required for _getch() to detect key presses

// ---------------------------------------------------------------------
//  VDTool (BETA)
//  A "Windows 95 Setup" themed front-end for yt-dlp / gallery-dl / ffmpeg
// ---------------------------------------------------------------------

#define NUM_OPTIONS 7

void gotoxy(int x, int y) {
    COORD coord;
    coord.X = x;
    coord.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

void setColor(int text, int background) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), text | (background << 4));
}

// Reliably force the console buffer AND window to exactly 80x25.
// Doing this with plain "mode con" can leave a mismatch between the
// screen buffer size and the window size if the terminal was already
// a different size (e.g. Windows Terminal), which causes wrapped /
// duplicated-looking text. Resizing in this specific order avoids that.
void resizeConsole(int cols, int rows) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    SMALL_RECT minRect = {0, 0, 1, 1};
    SetConsoleWindowInfo(hOut, TRUE, &minRect);

    COORD bufSize = {(SHORT)cols, (SHORT)rows};
    SetConsoleScreenBufferSize(hOut, bufSize);

    SMALL_RECT windowRect = {0, 0, (SHORT)(cols - 1), (SHORT)(rows - 1)};
    SetConsoleWindowInfo(hOut, TRUE, &windowRect);

    SetConsoleScreenBufferSize(hOut, bufSize);
}

void hideCursor() {
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    info.dwSize = 100;
    info.bVisible = FALSE;
    SetConsoleCursorInfo(consoleHandle, &info);
}

void showCursor() {
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    info.dwSize = 100;
    info.bVisible = TRUE;
    SetConsoleCursorInfo(consoleHandle, &info);
}

void drawBox(int x1, int y1, int x2, int y2) {
    gotoxy(x1, y1); printf("\xE2\x95\x94"); // ╔
    gotoxy(x2, y1); printf("\xE2\x95\x97"); // ╗
    gotoxy(x1, y2); printf("\xE2\x95\x9A"); // ╚
    gotoxy(x2, y2); printf("\xE2\x95\x9D"); // ╝

    for (int i = x1 + 1; i < x2; i++) {
        gotoxy(i, y1); printf("\xE2\x95\x90"); // ═
        gotoxy(i, y2); printf("\xE2\x95\x90"); // ═
    }
    for (int i = y1 + 1; i < y2; i++) {
        gotoxy(x1, i); printf("\xE2\x95\x91"); // ║
        gotoxy(x2, i); printf("\xE2\x95\x91"); // ║
    }
}

void drawStatusBar(const char *text) {
    gotoxy(0, 24);
    setColor(0, 7); // Black on light gray
    printf(" %-79s", text);
    setColor(15, 1);
}

void clearArea(int x1, int y1, int x2, int y2) {
    for (int y = y1; y <= y2; y++) {
        gotoxy(x1, y);
        for (int x = x1; x <= x2; x++) printf(" ");
    }
}

void ensureDownloadsFolder() {
    // %USERPROFILE%\Downloads already exists on virtually every Windows
    // install, but this is a harmless no-op safety net if it's missing.
    system("if not exist \"%USERPROFILE%/Downloads\" mkdir \"%USERPROFILE%/Downloads\"");
}

// Multi-URL queue box (up to 6 URLs, one per line). Long URLs scroll
// horizontally within their own row instead of spilling onto the next
// line, so each row always maps to exactly one queued URL.
// ENTER moves to the next URL slot. F2 finishes and continues.
// ESC cancels. Returns how many non-empty URLs were entered.
#define URL_MAX_LINES 6
#define URL_LINE_WIDTH 58   // visible field width
#define URL_LINE_CAP 512    // max characters stored per URL

static void redrawQueueLine(const char *text, int len, int row) {
    gotoxy(11, row);
    int start = (len > URL_LINE_WIDTH) ? len - URL_LINE_WIDTH : 0;
    printf("%-58s", text + start);
    gotoxy(11 + (len - start), row);
}

int promptInputURLQueue(const char *label, char urls[URL_MAX_LINES][URL_LINE_CAP]) {
    setColor(15, 1);
    gotoxy(10, 12);
    printf("%s", label);

    int boxTop = 14;
    int boxBottom = boxTop + 1 + URL_MAX_LINES; // 21

    drawBox(9, boxTop, 70, boxBottom);
    for (int i = 0; i < URL_MAX_LINES; i++) {
        gotoxy(11, boxTop + 1 + i);
        setColor(0, 15);
        printf("%-58s", "");
    }
    setColor(15, 1);
    gotoxy(10, boxBottom + 2);
    printf("One URL per line (up to 6) - they queue and run in order.");
    gotoxy(10, boxBottom + 3);
    printf("ENTER = next URL   F2 = Start Queue   ESC = Cancel");

    char lines[URL_MAX_LINES][URL_LINE_CAP];
    int lens[URL_MAX_LINES];
    for (int i = 0; i < URL_MAX_LINES; i++) {
        lines[i][0] = '\0';
        lens[i] = 0;
    }
    int curLine = 0;

    showCursor();
    setColor(0, 15);
    gotoxy(11, boxTop + 1);

    while (1) {
        int ch = _getch();

        if (ch == 0 || ch == 224) { // Extended key prefix (F-keys, arrows)
            int ch2 = _getch();
            if (ch2 == 60) break; // F2 scancode -> finish
            continue; // ignore other extended keys
        }
        if (ch == 27) { // ESC -> cancel
            for (int i = 0; i < URL_MAX_LINES; i++) {
                lines[i][0] = '\0';
                lens[i] = 0;
            }
            break;
        }
        if (ch == 13) { // ENTER -> next URL slot
            if (curLine < URL_MAX_LINES - 1) {
                curLine++;
                redrawQueueLine(lines[curLine], lens[curLine], boxTop + 1 + curLine);
            }
            continue;
        }
        if (ch == 8) { // Backspace
            if (lens[curLine] > 0) {
                lens[curLine]--;
                lines[curLine][lens[curLine]] = '\0';
            } else if (curLine > 0) {
                curLine--;
            }
            redrawQueueLine(lines[curLine], lens[curLine], boxTop + 1 + curLine);
            continue;
        }
        if (ch >= 32 && ch <= 126) { // Printable character
            if (lens[curLine] < URL_LINE_CAP - 1) {
                lines[curLine][lens[curLine]++] = (char)ch;
                lines[curLine][lens[curLine]] = '\0';
                redrawQueueLine(lines[curLine], lens[curLine], boxTop + 1 + curLine);
            }
        }
    }

    hideCursor();
    int count = 0;
    for (int i = 0; i < URL_MAX_LINES; i++) {
        if (lens[i] > 0) {
            strcpy(urls[count], lines[i]);
            count++;
        }
    }
    setColor(15, 1);
    return count;
}

// Simple line-input box under a prompt. Uses the console's own line
// editing (backspace etc.) since we just drop into scanf-land.
void promptInput(const char *label, char *buffer, int maxlen) {
    setColor(15, 1);
    gotoxy(10, 12);
    printf("%s", label);
    drawBox(9, 14, 70, 16);
    gotoxy(11, 15);
    setColor(0, 15);
    printf("%-58s", ""); // blank field
    gotoxy(11, 15);
    showCursor();
    fflush(stdin);
    fgets(buffer, maxlen, stdin);
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') buffer[len - 1] = '\0';
    hideCursor();
    setColor(15, 1);
}

// "Please wait" screen shown while a blocking system() call runs.
// waitForKeypress: if 1, blocks on a keypress before returning (used for
// single, non-queued runs, and for the last item in a queue). If 0, just
// shows the result briefly and returns immediately so a queue can keep
// rolling on to the next item without the user having to press anything.
void showWorking(const char *toolName, const char *cmd, int waitForKeypress) {
    system("cls");
    setColor(15, 1);
    gotoxy(23, 2);  printf("Please wait while MediaTool runs");
    gotoxy(24, 3);  printf("your request using %s.", toolName);
    drawBox(8, 8, 72, 14);
    gotoxy(10, 9);  printf("Running:");
    gotoxy(10, 10); printf("%.60s", cmd);
    gotoxy(10, 12); printf("This window may look frozen - that's just the tool working.");
    drawStatusBar(" Please wait...                                                              ");

    int result = system(cmd);

    clearArea(9, 9, 71, 12);
    gotoxy(10, 9);
    if (result == 0) {
        setColor(10, 1);
        printf("Done! The command finished successfully.");
    } else {
        setColor(12, 1);
        printf("The command exited with an error (code %d).", result);
    }
    setColor(15, 1);
    gotoxy(10, 11);
    printf("Check %%USERPROFILE%%\\Downloads (or your chosen output path).");

    if (waitForKeypress) {
        gotoxy(25, 21);
        printf("Press any key to return to the main menu.");
        _getch();
    } else {
        gotoxy(15, 21);
        printf("Continuing to the next item in the queue...");
        Sleep(900);
    }
}

// Interactive checkbox screen shown before a yt-dlp download starts.
// Move with arrows, toggle a checkbox with SPACE or ENTER, and either
// select "Start Download" or "Cancel". Returns 1 to proceed, 0 to abort.
int showDownloadOptions(int *audioOnly, int *videoFormat, int *embedMeta, int *embedThumb) {
    const int NUM_CHECKBOXES = 4;
    int cursor = 0; // 0-3 = checkboxes, 4 = Start, 5 = Cancel
    int keyPressed;
    const char *labels[4] = {
        "Audio only (extract audio as mp3)",
        "MP4 format (please uncheck audio only)",
        "Include all metadata (title, artist, etc.)",
        "Include thumbnail"
    };
    int *values[4] = { audioOnly, videoFormat, embedMeta, embedThumb };

    while (1) {
        system("cls");
        setColor(15, 1);
        gotoxy(5, 2);  printf("Download Options");
        gotoxy(5, 3);  printf("================");
        gotoxy(5, 5);  printf("Move with arrows. ENTER or SPACE toggles a box.");

        for (int i = 0; i < NUM_CHECKBOXES; i++) {
            gotoxy(10, 8 + i * 2);
            if (i == cursor) setColor(0, 15); else setColor(15, 1);
            printf("[%c] %-44s", *values[i] ? 'X' : ' ', labels[i]);
            setColor(15, 1);
        }

        gotoxy(10, 18);
        if (cursor == NUM_CHECKBOXES) setColor(0, 15); else setColor(15, 1);
        printf("%-30s", "> Start Download");
        setColor(15, 1);

        gotoxy(10, 20);
        if (cursor == NUM_CHECKBOXES + 1) setColor(0, 15); else setColor(15, 1);
        printf("%-30s", "< Cancel");
        setColor(15, 1);

        drawStatusBar(" ENTER=Toggle/Select   Arrows=Move   ESC=Cancel                              ");

        keyPressed = _getch();
        if (keyPressed == 224) { // Arrow key prefix
            keyPressed = _getch();
            if (keyPressed == 72) cursor = (cursor == 0) ? (NUM_CHECKBOXES + 1) : cursor - 1; // Up
            else if (keyPressed == 80) cursor = (cursor == NUM_CHECKBOXES + 1) ? 0 : cursor + 1; // Down
        } else if (keyPressed == 27) { // ESC
            return 0;
        } else if (keyPressed == ' ') {
            if (cursor < NUM_CHECKBOXES) *values[cursor] = !*values[cursor];
        } else if (keyPressed == 13) { // ENTER
            if (cursor < NUM_CHECKBOXES) *values[cursor] = !*values[cursor];
            else if (cursor == NUM_CHECKBOXES) return 1;
            else return 0;
        }
    }
}

void screenDownloadVideo() {
    system("cls");
    setColor(15, 1);
    gotoxy(5, 2);  printf("Download Video");
    gotoxy(5, 3);  printf("==============");
    gotoxy(5, 5);  printf("Paste one or more video URLs below (YouTube, Twitter/X, etc).");
    gotoxy(5, 6);  printf("They'll be saved into %%USERPROFILE%%\\Downloads via yt-dlp.");

    char urls[URL_MAX_LINES][URL_LINE_CAP];
    int count = promptInputURLQueue("Video URL(s):", urls);
    if (count == 0) return;

    int audioOnly = 0, videoFormat = 0, embedMeta = 0, embedThumb = 0;
    if (!showDownloadOptions(&audioOnly, &videoFormat, &embedMeta, &embedThumb)) return;

    ensureDownloadsFolder();

    for (int i = 0; i < count; i++) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd),
                 "yt-dlp -o \"%%USERPROFILE%%/Downloads/%%(title)s.%%(ext)s\"");

        if (audioOnly)   strcat(cmd, " -x --audio-format mp3");
        if (videoFormat)  strcat(cmd, " -f mp4");
        if (embedMeta)   strcat(cmd, " --embed-metadata");
        if (embedThumb)  strcat(cmd, " --embed-thumbnail");

        strcat(cmd, " \"");
        strcat(cmd, urls[i]);
        strcat(cmd, "\"");

        char label[64];
        snprintf(label, sizeof(label), "yt-dlp  (item %d of %d)", i + 1, count);

        // Only pause for a keypress on the last item, so the queue
        // rolls straight through without needing input in between.
        showWorking(label, cmd, i == count - 1);
    }
}

// Simple single-checkbox toggle screen (same look/feel as
// showDownloadOptions but for cases with only one setting to flip).
int showSingleToggle(const char *label, int *value) {
    int cursor = 0; // 0 = checkbox, 1 = Start, 2 = Cancel
    int keyPressed;

    while (1) {
        system("cls");
        setColor(15, 1);
        gotoxy(5, 2);  printf("Download Options");
        gotoxy(5, 3);  printf("================");
        gotoxy(5, 5);  printf("Move with arrows. ENTER or SPACE toggles a box.");

        gotoxy(10, 8);
        if (cursor == 0) setColor(0, 15); else setColor(15, 1);
        printf("[%c] %-44s", *value ? 'X' : ' ', label);
        setColor(15, 1);

        gotoxy(10, 16);
        if (cursor == 1) setColor(0, 15); else setColor(15, 1);
        printf("%-30s", "> Start Download");
        setColor(15, 1);

        gotoxy(10, 18);
        if (cursor == 2) setColor(0, 15); else setColor(15, 1);
        printf("%-30s", "< Cancel");
        setColor(15, 1);

        drawStatusBar(" ENTER=Toggle/Select   Arrows=Move   ESC=Cancel                              ");

        keyPressed = _getch();
        if (keyPressed == 224) {
            keyPressed = _getch();
            if (keyPressed == 72) cursor = (cursor == 0) ? 2 : cursor - 1;
            else if (keyPressed == 80) cursor = (cursor == 2) ? 0 : cursor + 1;
        } else if (keyPressed == 27) {
            return 0;
        } else if (keyPressed == ' ') {
            if (cursor == 0) *value = !*value;
        } else if (keyPressed == 13) {
            if (cursor == 0) *value = !*value;
            else if (cursor == 1) return 1;
            else return 0;
        }
    }
}

void screenDownloadImages() {
    system("cls");
    setColor(15, 1);
    gotoxy(5, 2);  printf("Download Images / Galleries");
    gotoxy(5, 3);  printf("============================");
    gotoxy(5, 5);  printf("Paste one or more gallery/post URLs below.");
    gotoxy(5, 6);  printf("They'll be saved into %%USERPROFILE%%\\Downloads via gallery-dl.");

    char urls[URL_MAX_LINES][URL_LINE_CAP];
    int count = promptInputURLQueue("Gallery URL(s):", urls);
    if (count == 0) return;

    int writeMeta = 0;
    if (!showSingleToggle("Include all metadata (saves a .json alongside)", &writeMeta)) return;

    ensureDownloadsFolder();

    for (int i = 0; i < count; i++) {
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "gallery-dl -d \"%%USERPROFILE%%/Downloads\"");
        if (writeMeta) strcat(cmd, " --write-metadata");
        strcat(cmd, " \"");
        strcat(cmd, urls[i]);
        strcat(cmd, "\"");

        char label[64];
        snprintf(label, sizeof(label), "gallery-dl  (item %d of %d)", i + 1, count);

        // Only pause for a keypress on the last item.
        showWorking(label, cmd, i == count - 1);
    }
}

void screenConvertMedia() {
    system("cls");
    setColor(15, 1);
    gotoxy(5, 2);  printf("Convert Video / Image");
    gotoxy(5, 3);  printf("======================");
    gotoxy(5, 5);  printf("Enter the input file path, then the desired output file path.");
    gotoxy(5, 6);  printf("Example: input.mkv  ->  output.mp4");

    char input[512], output[512];
    promptInput("Input file path:", input, sizeof(input));
    if (strlen(input) == 0) return;

    system("cls");
    setColor(15, 1);
    gotoxy(5, 2);  printf("Convert Video / Image");
    gotoxy(5, 3);  printf("======================");
    promptInput("Output file path (with extension):", output, sizeof(output));
    if (strlen(output) == 0) return;

    char cmd[1200];
    snprintf(cmd, sizeof(cmd), "ffmpeg -y -i \"%s\" \"%s\"", input, output);
    showWorking("ffmpeg", cmd, 1);
}

void screenGifCreator() {
    system("cls");
    setColor(15, 1);
    gotoxy(5, 2);  printf("GIF Creator");
    gotoxy(5, 3);  printf("===========");
    gotoxy(5, 5);  printf("Enter the input video path, then the output .gif path.");
    gotoxy(5, 6);  printf("A clean 10fps / 480px-wide GIF will be generated with ffmpeg.");

    char input[512], output[512];
    promptInput("Input video path:", input, sizeof(input));
    if (strlen(input) == 0) return;

    system("cls");
    setColor(15, 1);
    gotoxy(5, 2);  printf("GIF Creator");
    gotoxy(5, 3);  printf("===========");
    promptInput("Output GIF path (e.g. output.gif):", output, sizeof(output));
    if (strlen(output) == 0) return;

    char cmd[1200];
    snprintf(cmd, sizeof(cmd),
             "ffmpeg -y -i \"%s\" -vf \"fps=10,scale=480:-1:flags=lanczos\" \"%s\"",
             input, output);
    showWorking("ffmpeg", cmd, 1);
}

void screenExtractAudio() {
    system("cls");
    setColor(15, 1);
    gotoxy(5, 2);  printf("Audio Extractor");
    gotoxy(5, 3);  printf("===============");
    gotoxy(5, 5);  printf("Enter the input video path, then the output audio path.");
    gotoxy(5, 6);  printf("Example output: song.mp3");

    char input[512], output[512];
    promptInput("Input video path:", input, sizeof(input));
    if (strlen(input) == 0) return;

    system("cls");
    setColor(15, 1);
    gotoxy(5, 2);  printf("Audio Extractor");
    gotoxy(5, 3);  printf("===============");
    promptInput("Output audio path (e.g. song.mp3):", output, sizeof(output));
    if (strlen(output) == 0) return;

    char cmd[1200];
    snprintf(cmd, sizeof(cmd),
             "ffmpeg -y -i \"%s\" -vn -q:a 0 \"%s\"", input, output);
    showWorking("ffmpeg", cmd, 1);
}

void screenUpdateDependencies() {
    system("cls");
    system("color 04");
    resizeConsole(95, 35);
    setColor(4, 0);
    gotoxy(5, 2);  printf("Update / Install Dependencies");
    gotoxy(5, 3);  printf("==============================");
    gotoxy(5, 5);  printf("This will ensure yt-dlp, gallery-dl, and ffmpeg are installed / updated.");
    gotoxy(5, 6);  printf("It is recommended to use Windows 11. Cause this uses winget for its sources.");
    gotoxy(5, 7);  printf("Windows 10 or lower dont have winget by default, so you will have to install it manually.");
    gotoxy(5, 9);  printf("To install winget on Windows 10. Update App Installer from the Microsoft Store.\n\n");

    system("pause");
    gotoxy(5, 11);  printf("\n\nPlease wait while the process runs...\n");
    system("winget update");
    system("winget install yt-dlp.yt-dlp mikf.gallery-dl Gyan.ffmpeg");
    system("cls");

    gotoxy(5, 8);
    setColor(10, 1);
    printf("All dependencies are installed and up to date.");
    setColor(15, 1);
    gotoxy(5, 10);
    printf("Press any key to return to the main menu.");
    resizeConsole(80, 25);
    _getch();
}

int main() {
    system("cls");
    SetConsoleOutputCP(CP_UTF8);
    resizeConsole(80, 25);
    system("color 1F");
    system("title VDTool (BETA)");
    hideCursor();

    char options[NUM_OPTIONS][40] = {
        "  Download Video               ",
        "  Download Images / Gallery    ",
        "  Convert Video / Image        ",
        "  Create GIF from Video        ",
        "  Extract Audio from Video     ",
        "  Update / Install Dependencies",
        "  Quit VDTool                  "
    };

    int selected = 0;
    int keyPressed = 0;

    while (1) {
        system("cls");
        setColor(15, 1);
        gotoxy(5, 2);  printf("VDTool (BETA)");
        gotoxy(5, 3);  printf("===============");
        gotoxy(5, 5);  printf("Welcome to VDTool.");
        gotoxy(5, 7);  printf("This little front-end drives yt-dlp, gallery-dl and ffmpeg");

        for (int i = 0; i < NUM_OPTIONS; i++) {
            gotoxy(10, 10 + (i * 2));
            if (i == selected) {
                setColor(0, 15);
                printf("%s", options[i]);
                setColor(15, 1);
            } else {
                printf("%s", options[i]);
            }
        }

        drawStatusBar(" ENTER=Select   \x18\x19=Move   ESC/F3=Quit                                     ");

        keyPressed = _getch();

        if (keyPressed == 224) { // Arrow key prefix
            keyPressed = _getch();
            if (keyPressed == 72) { // Up
                selected = (selected == 0) ? NUM_OPTIONS - 1 : selected - 1;
            } else if (keyPressed == 80) { // Down
                selected = (selected == NUM_OPTIONS - 1) ? 0 : selected + 1;
            }
        } else if (keyPressed == 27) { // ESC
            break;
        } else if (keyPressed == 13) { // ENTER
            switch (selected) {
                case 0: screenDownloadVideo();   break;
                case 1: screenDownloadImages();  break;
                case 2: screenConvertMedia();    break;
                case 3: screenGifCreator();      break;
                case 4: screenExtractAudio();    break;
                case 5: screenUpdateDependencies(); break;
                case 6: return 0;
            }
        }
    }

    system("cls");
    system("color 07");
    setColor(7, 0);
    return 0;
}