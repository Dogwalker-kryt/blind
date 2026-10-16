#define _POSIX_C_SOURCE 200809L

/*
 * Blind - A simple program to practice touch-typing in the terminal.
 *
 * Practice touch-typing speed, right on the terminal using either an existing
 * presets, or a provided text. Provided text can be either string argument or
 * a full text file from any kind. WPM and accuracy or errors, are calculated
 * and shown at the end.
 *
 * Keys:
 *   <RETURN>             ENTER to skip a line
 *   <CTRL-C>             EXIT at any time
 *
 * Options:
 *   -h                   display help message
 *   -v                   show version number
 *   -b                   block on wrong typing
 *   -f FILE              practice on FILE lines
 *   -l N                 file line number to start on
 *
 *   --help               display help message
 *   --block              block on wrong typing
 *   --version            show version number
 *   --file=FILE          practice on FILE lines
 *   --start-line=N       file line number to start on
 *   --show-actual        show the actual typed letter
 *   --allow-back         allow backspace for correction
 *   --alt-screen         run the program in alt-screen
 *
 * Arguments:
 *   STRING               one line of text. quoted or unquoted.
 *   FILE                 any file containing text
 *
 * Example:
 *   blind -b
 *   blind \"line to practice on\"
 *   blind --file <path-to-file> -l 10
 *
 * repo: https://github.com/shalom2552/blind.git
 * author: shalom2552
 * date: 2026-09-24
 */
#include <assert.h>
#include <complex.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>

const char* NAME;
#define VERSION "0.2.6"
#define MAX_TEXT_LINE_LEN 2048

#define no_display() getenv("NDISPLAY")
#define display(...) do { if (!no_display()) printf(""__VA_ARGS__); fflush(stdout); } while (0)
#define DEBUG(...)   fprintf(stderr, "DEBUG: "__VA_ARGS__)

#define RED   "\033[31m"
#define GREEN "\033[32m"
#define UNDER "\033[4m"
#define DIM   "\033[2m"
#define BOLD  "\033[1m"
#define BACK  "\033[D"
#define RST   "\033[0m"

enum Options {
    OPT_SHOW_ACTUAL = 256,
    OPT_ALLOW_BACKSPACE,
    OPT_ALT_SCREEN,
};

enum InputMode {
    DEFAULT_INPUT,
    STRING_INPUT,
    FILE_INPUT
};

enum OutputMode {
    DEFAULT_OUTPT,
    CUSTOM_PATH_OUTPUT
};

int input_mode = DEFAULT_INPUT;
int blocking_mode          = 0;
int allow_backspace_mode   = 0;
int show_actual_typed_mode = 0;
int hide_line_score        = 0;
int run_in_alt_screen      = 0;
int file_start_line        = 1;

char* input_file_path   = NULL;
char* input_string_line = NULL;

#define DEFAULT_OUTPUT_PATH "/.local/share/blind-tt/blind_stats.csv"
int output_mode = DEFAULT_OUTPT;
char* output_file_path = NULL;

int term_initialized = 0;
struct termios term;

struct {
    int wpm;
    int typed_errors;
    int lines_count;
    int lines_skipped;
    int total_letters;
    int correct_letters;
    double accuracy;
} score = {0};

// https://clagnut.com/blog/2380
static char* Data[] = {
    "Who packed five dozen old quart jugs in my box?",
    "The quick brown fox jumped over the lazy dogs.",
    "Woven silk pyjamas exchanged for blue quartz.",
    "Brawny gods just flocked up to quiz and vex him.",
    "Twelve ziggurats quickly jumped a finch box.",
    "Prating jokers quizzically vexed me with fibs.",
    "Amazingly few discotheques provide jukeboxes.",
    "The quick onyx goblin jumps over the lazy dwarf.",
    "The lazy major was fixing Cupid's broken quiver.",
    "My faxed joke won a pager in the cable TV quiz show.",
    "Jacky can now give six big tips from the old quiz.",
    0
};

void version(void)
{
    fprintf(stderr, "%s: %s\n", NAME, VERSION);
}

void usage(void)
{
    fprintf(stderr, "%s: usage: %s [OPTIONS] [STRING|FILE]\n", NAME, NAME);
}

void help(void)
{
    fprintf(stderr,
        "A program to practice touch-typing in the terminal.\n"
        "\n"
        "  Practice touch-typing speed, right on the terminal\n"
        "  using either an existing presets, or a provided text.\n"
        "  Provided text can be either string argument or a full\n"
        "  text file from any kind. WPM and accuracy or errors, are\n"
        "  calculated and shown at the end.\n"
        "\n"
        "Keys:\n"
        "  <RETURN>             ENTER to skip a line\n"
        "  <CTRL-C>             EXIT at any time\n"
        "\n"
        "Options:\n"
        "  -h                   display help message\n"
        "  -v                   show version number\n"
        "  -b                   block on wrong typing\n"
        "  -f FILE              practice on FILE lines\n"
        "  -l N                 file line number to start on\n"
        "  -o[FILE]             specifiy FILE to store stats (default: $HOME" DEFAULT_OUTPUT_PATH ")\n"
        "\n"
        "  --help               display help message\n"
        "  --block              block on wrong typing\n"    
        "  --version            show version number\n"
        "  --file=FILE          practice on FILE lines\n"
        "  --start-line=N       file line number to start on\n"
        "  --show-actual        show the actual typed letter\n"
        "  --allow-back         allow backspace for correction\n"
        "  --alt-screen         run the program in alt-screen\n"
        "  --output[=FILE]      specifiy FILE to store stats (default: $HOME" DEFAULT_OUTPUT_PATH  ")\n"
        "                       Path format is /path/fil_name.csv\n"
        "\n"
        "Arguments:\n"
        "  STRING               one line of text. quoted or unquoted.\n"
        "  FILE                 any file containing text\n"
        "\n"
        "Example:\n"
        "  blind -b\n"
        "  blind -o\n"
        "  blind -ostats.csv \"line to practice on\"\n"
        "  blind --output=stats.csv \"line to practice on\"\n"
        "  blind \"line to practice on\"\n"
        "  blind --file <path-to-file> -l 10\n"
        "\n"
        "Bugs can be reported on GitHub:\n"
        "https://github.com/shalom2552/blind.git\n"
    );
}

void print_result(int wpm, double accuracy, int errors)
{
    display(DIM"WPM:      "RST"%d\n"RST, wpm);
    if (blocking_mode) {
        display(DIM"Errors:   "RST"%s%d\n"RST, errors == 0 ? GREEN : RED, errors);
    } else {
        display(DIM"Accuracy: "RST"%s%.2f%%\n"RST, accuracy > 75 ? GREEN : RED, accuracy);
    }
}

void print_totals(void)
{
    if (run_in_alt_screen) display("\033[?1049l"); // exit alt-screen
    display(DIM"\n=============== Sumary ===============\n");
    print_result(score.wpm, score.accuracy, score.typed_errors);
    if (score.lines_count > 1) display(DIM"Lines:    "RST"%d\n", score.lines_count);
    if (score.lines_skipped) display(DIM"Skipped:  "RST"%d\n", score.lines_skipped);
    if (run_in_alt_screen) display("\033[?1049h"); // back to alt-screen
}

void play(char* s)
{
    tcflush(STDIN_FILENO, TCIFLUSH);              // flush the input buffer
    struct winsize w; ioctl(0, TIOCGWINSZ, &w);   // get terminal size
    if (strlen(s) > w.ws_col) s[w.ws_col] = '\0'; // trim the line
    display(DIM"\n%s\r"RST, s);                   // print dimmed line

    int idx     = 0;
    int correct = 0;
    int errors  = 0;
    int total   = 0;
    struct timespec start, end;
    char* history = (char*) malloc(strlen(s));
    assert(history && "Buy more RAM");

    char* c = strchr(s, '\n');
    if (c) *c = '\0';

    // skip spaces and tabs prefix
    for (; isspace(s[idx]); ++idx) {
        display(" ");
    }

    while (s[idx] != '\0') {

        // line end with space
        if (isspace(s[idx]) && s[idx + 1] == '\0') {
            ++idx; break;
        }

        // skip long spaces and tabs
        if (idx > 0 && isspace(s[idx - 1]) && isspace(s[idx])) {
            ++idx; display(" "); continue;
        }

        char c = getchar();

        // start the clock on first type (after getchar)
        if (!total) clock_gettime(CLOCK_MONOTONIC, &start);

        if (c == '\n') { // skip line on enter
            score.lines_skipped += 1;
            total = 0;
            break;

        } else if (c == 27) { // skip escape keys
            getchar(); getchar(); continue;

        } else if (c == 127) { // backspace: backtrack
            if (idx > 0 && allow_backspace_mode) {
                --idx;
                --total;
                if (history[idx] == s[idx]) {
                    --correct;
                } else {
                    --errors;
                }
                display(BACK DIM"%c"BACK RST, s[idx]);
            }
            continue;
        }

        if (c == s[idx]) {
            ++correct;
            display(GREEN"%c"RST, c);
        } else {
            ++errors;
            if (blocking_mode) {
                continue;
            }
            display(RED"%c"RST, show_actual_typed_mode ? c : s[idx]);
        }

        fflush(stdout);
        history[idx++] = c;
        ++total;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    // update scores if line started
    if (total > 1) {
        double time     = (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;
        double accuracy = (double)correct / total * 100;
        int wpm         = ((double)total / 5.0) / (time / 60.0);

        score.lines_count     += 1;
        score.total_letters   += total;
        score.correct_letters += correct;
        score.typed_errors    += errors;
        score.accuracy        =  (double)score.correct_letters / score.total_letters * 100;
        score.wpm             =  (score.wpm * score.lines_count + wpm) / (score.lines_count + 1);

        if (!hide_line_score) {
            display("\n\n");
            print_result(wpm, accuracy, errors);
        }
    }
}

void run_presets(void)
{
    int i = 0;
    while (Data[i] != 0) {
        play(Data[i++]);
        display("\n");
    }
    print_totals();
}

void run_provided_text(int argc, char** argv)
{
    // play rest of cmdline args as text
    char text[MAX_TEXT_LINE_LEN] = "";
    while (optind < argc) {
        strcat(text, argv[optind++]);

        // seprate arguments by space
        if (optind < argc) {
            strcat(text, " ");
        }
    }

    play(text);
}

void run_file_input(void)
{
    FILE*   finput;
    char*   line   = NULL;
    int     line_c = 1;
    size_t  size   = 0;
    ssize_t nread;

    finput = fopen(input_file_path, "r");
    if (!finput) {
        fprintf(stderr, "%s: %s: %s\n", NAME, strerror(errno), input_file_path);
        return;
    }

    // play each line of the file
    while ((nread = getline(&line, &size, finput)) != -1) {
        if (++line_c < file_start_line) continue;

        // only run regular lines (ending with '\n')
        char* c = strchr(line, '\n');
        if (c) {
            *c = '\0';
            play(line);
        }
    }

    free(line);
    fclose(finput);
    print_totals();
}

int ensure_dir_exists(const char *path) {
    char temp[PATH_MAX];
    char *p = NULL;

    if (strlen(path) >= PATH_MAX) {
        return -1;
    }

    strncpy(temp, path, PATH_MAX);
    
    for (p = temp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            // 0755: owner rwx, group rx, others rx
            if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            *p = '/';
        }
    }

    if (mkdir(temp, 0755) != 0 && errno != EEXIST) {
        return -1;
    }

    return 0;
}

FILE* open_for_append(const char* full_path) {
    if (full_path == NULL || strlen(full_path) >= PATH_MAX) {
        return NULL;
    }

    char path_copy[PATH_MAX];
    strncpy(path_copy, full_path, PATH_MAX);

    char *last_slash = strrchr(path_copy, '/');
    if (last_slash != NULL) {
        *last_slash = '\0'; 

        if (ensure_dir_exists(path_copy) != 0) {
            perror("HR Error: Failed to create directory tree");
            return NULL;
        }
    }

    return fopen(full_path, "a");
}


void gen_output_file_path(char* custom_output_path) {
    char* home_usr = getenv("HOME");
    static char buff[256];

    strncpy(buff, home_usr, sizeof(buff));
    buff[sizeof(buff) - 1] = '\0';

    char* temp_file_path = DEFAULT_OUTPUT_PATH;
    if (output_mode == CUSTOM_PATH_OUTPUT) temp_file_path = custom_output_path;

    strcat(buff, temp_file_path);

    output_file_path = buff;
}

void write_summary_to_file(void) {
    gen_output_file_path(output_file_path);

    FILE* output_file = open_for_append(output_file_path);

    long file_size = ftell(output_file);
    if (file_size < 0) {
        fprintf(stderr, "%s: cannot get output file size for '%s': %s\n",
            NAME, output_file_path, strerror(errno)
        );
        fclose(output_file);
        return;
    }

    const char *mode;
    switch (input_mode) {
        case STRING_INPUT: mode = "text"; break;
        case FILE_INPUT:   mode = "file"; break;
        case DEFAULT_INPUT: mode = "unknown"; break;
        default:           mode = "presets"; break;
    }

    int csv_errors = blocking_mode ? score.typed_errors : -1;
    double csv_accuracy = blocking_mode ? -1.0 : score.accuracy;

    if (file_size == 0 &&
        fprintf(output_file, "timestamp,mode,lines,wpm,errors,accuracy,correct_letters,lines_skipped,total_letters\n") < 0) {
        fprintf(stderr, "%s: cannot write CSV header to '%s': %s\n",
            NAME, output_file_path, strerror(errno)
        );
        fclose(output_file);
        return;
    }

    int fprintf_res = fprintf(output_file, "%lld,%s,%d,%d,%d,%.2f,%d,%d,%d\n",
                (long long)time(NULL), mode, score.lines_count, score.wpm, 
                csv_errors, csv_accuracy, score.correct_letters, score.lines_skipped, score.total_letters
    );

    if (fprintf_res < 0) {
        fprintf(stderr, "%s: cannot write session statistics to '%s': %s\n",
            NAME, output_file_path, strerror(errno)
        );
        fclose(output_file);
        return;
    }

    if (fclose(output_file) != 0) {
        fprintf(stderr, "%s: cannot close output file '%s': %s\n",
            NAME, output_file_path, strerror(errno)
        );
    }
}

void init(void)
{
    if (!term_initialized && !no_display()) {
        tcgetattr(STDIN_FILENO, &term);
        struct termios raw = term;
        raw.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        if (run_in_alt_screen) display("\033[?1049h");
    } else {
        DEBUG("running in NDISPLAY mode\n");
    }
    term_initialized = 1;
}

void cleanup(void)
{
    if (term_initialized && !no_display()) {
        if (run_in_alt_screen) display("\033[?1049l");
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);
    }
}

void handle_siginit(int sig)
{
    print_totals();
    (void)sig;
    exit(0);
}

void parse_args(int argc, char** argv)
{
    char* laststash = strrchr(*argv, '/');
    NAME = laststash ? laststash + 1 : *argv;

    int c;
    for (;;) {
        int option_idx;
        static struct option lo[] = {
            { "help",        no_argument,       0, 'h'                 },
            { "version",     no_argument,       0, 'v'                 },
            { "block",       no_argument,       0, 'b'                 },
            { "file",        required_argument, 0, 'f'                 },
            { "start-line",  required_argument, 0, 'l'                 },
            { "allow-back",  no_argument,       0, OPT_ALLOW_BACKSPACE },
            { "show-actual", no_argument,       0, OPT_SHOW_ACTUAL     },
            { "alt-screen",  no_argument,       0, OPT_ALT_SCREEN      },
            { "output",      optional_argument, 0, 'o'                 },
            {0}
        };

        c = getopt_long(argc, argv, "hbvo::f:l:", lo, &option_idx);
        if (c == -1) break;

        switch (c) {
            case 'h':
                help();
                exit(0);

            case 'v':
                version();
                exit(0);

            case 'b':
                blocking_mode = 1;
                break;

            case 'l':
                file_start_line = atoi(optarg);
                break;

            case 'f':
                input_mode = FILE_INPUT;
                input_file_path = optarg;
                hide_line_score = 1;
                break;

            case 'o':
                output_mode = CUSTOM_PATH_OUTPUT;
                if (optarg) {
                    char *fname = optarg;

                    if (*fname == '=') {
                        ++fname;
                    }

                    if (*fname == '\0') {
                        fprintf(stderr, "%s: output filename cannot be empty\n", NAME);
                        exit(1);
                    }
                    output_file_path = fname;
                }
                break;

            case OPT_ALLOW_BACKSPACE:
                allow_backspace_mode = 1;
                break;

            case OPT_SHOW_ACTUAL:
                show_actual_typed_mode = 1;
                break;

            case OPT_ALT_SCREEN:
                run_in_alt_screen = 1;
                break;

            default:
                usage();
                exit(1);
        }
    }
}

int main(int argc, char** argv)
{
    atexit(cleanup);
    atexit(write_summary_to_file);

    signal(SIGINT, handle_siginit);

    parse_args(argc, argv);
    init();

    // HACK: assuming rest of args are the text input if no input mode provided
    //          this allows to run unquoted text: `blind line to practice`
    if (optind < argc && input_mode == DEFAULT_INPUT) {
        input_mode = STRING_INPUT;
    }

    switch (input_mode) {
        case STRING_INPUT : run_provided_text(argc, argv); break;
        case FILE_INPUT   : run_file_input()             ; break;
        case DEFAULT_INPUT: run_presets()                ; break;
    }

    return 0;
}

//
// FIX: BUGS
//
//      BUG: 10, research
//          does ioctl() (to get the cols size for wraping line), is POSIX support?
//          google search says to include <sys/ioctl.h>, but manpage says <stropts.h>
//          which dosen't compile.
//
// NOTE: FEATURES
//
//      NOTE: 30, easy
//          on wrong typed space ' ' add an indication (maybe '_' or red bg ' ')
//          if using '_':
//              con: it can clash with real '_' character, lines may already have it.
//              pro: no need to use bg color.
//          if using bg color: neesd to add red bg color
//          in both cases needs to print wehter its actual-typed mode or not,
//              we dont want to change the original string `s`.
//
//      NOTE: 40, small
//          hide cursor and blink current word.
//
//      NOTE: 30, medium
//          caculate mid line scrors (WPM is the issue, how to update mid line?)
//          or document that mid line enter not save stats.
//
//      NOTE: 50, medium
//          output stats as a json or csv format, so i can track my progress:
//              {#lines, WPM, [accuracy], [errors]}
//          either to the terminal or appending to a predefined file
//          (or to a provided one with flag? i dont want to preovide a flag every
//              time, just to track my progress).
//          this tool is not about parsing this output or listing agragations,
//          this is a blind typing, so just outputing it is sufficient.
//
