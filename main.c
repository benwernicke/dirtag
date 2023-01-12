#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "flag.h"

static bool  flag_help      = 0;
static char* config_file    = ".dirtag";
static bool  flag_add_tag   = 0;
static bool  flag_clear_tag = 0;
static bool  flag_find      = 0;
static char* delimeter      = "\n";
static bool  flag_rm_file   = 0;
static bool  flag_new_file  = 0;

#define PANIC(...)                      \
    do {                                \
        fprintf(stderr, "Error: ");    \
        fprintf(stderr, __VA_ARGS__);   \
        fprintf(stderr, "\n");          \
        exit(1);                        \
    } while(0)

#define PANIC_IF(b, ...) do { if (b) {PANIC(__VA_ARGS__);} } while (0)

typedef struct line_t line_t;
struct line_t {
    char*    file;
    char*    tags;
    uint32_t size;
    uint32_t cap;
};

typedef struct dirtag_t dirtag_t;
struct dirtag_t {
    char*    file_content;
    line_t*  buf;
    uint32_t cap;
    uint32_t size;
};

static dirtag_t dirtag = { 0 };

static line_t* dirtag_begin(void) { return dirtag.buf; }
static line_t* dirtag_end(void) { return dirtag.buf + dirtag.size; }

static line_t* dirtag_more(void)
{
    if (dirtag.size >= dirtag.cap) {
        dirtag.cap <<= 1;
        dirtag.cap  += 1;
        dirtag.buf = realloc(dirtag.buf, dirtag.cap * sizeof(*dirtag.buf));
        PANIC_IF(!dirtag.buf, "bad malloc");
    }
    return &dirtag.buf[dirtag.size++];
}

static char* triml(char* s)
{
    for (; isspace(*s); ++s) {  }
    return s;
}

static void load_config(void)
{
    // read file
    {
        FILE* f = fopen(config_file, "r");
        PANIC_IF(!f, "could not open file %s:%s", config_file, strerror(errno));
        fseek(f, 0, SEEK_END);
        uint64_t l = ftell(f);
        rewind(f);
        dirtag.file_content = malloc(l + 1);
        dirtag.file_content[l] = 0;
        fread(dirtag.file_content, 1, l, f);
        fclose(f);
    }

    char* s = dirtag.file_content;
    while (s && *s) {
        s = triml(s);
        line_t* l = dirtag_more();
        l->file = s;
        for (; *s && !isspace(*s); ++s) {  }
        PANIC_IF(!*s, "file %s wrongly formatted", config_file);
        *s++ = 0;
        s = triml(s);
        l->tags = s;
        for (; *s && !isspace(*s); ++s) {  }
        s = *s ? *s++ = 0, s : s;
    }
}

static void deinit(void)
{
    free(dirtag.file_content);
    free(dirtag.buf);
    memset(&dirtag, 0, sizeof(dirtag));
}

static void add_tag(char* file, char* tag)
{
    FILE* f = fopen(config_file, "w");
    PANIC_IF(!f, "could not open file %s: '%s'", config_file, strerror(errno));
    line_t* iter = dirtag_begin();
    line_t* end  = dirtag_end();

    for (; iter != end; ++iter) {
        fprintf(f, "%s %s", iter->file, iter->tags);
        if (strcmp(iter->file, file) == 0) {
            fprintf(f, ":%s", tag);
        }
        fprintf(f, "\n");
    }

    fclose(f);
}

static void clear_tag(char* file)
{
    FILE* f = fopen(config_file, "w");
    PANIC_IF(!f, "could not open file %s: '%s'", config_file, strerror(errno));
    line_t* iter = dirtag_begin();
    line_t* end  = dirtag_end();

    for (; iter != end; ++iter) {
        fprintf(f, "%s %s\n",
                iter->file,
                strcmp(iter->file, file) == 0 ? "all" : iter->tags
            );
    }

    fclose(f);
}

static bool is_tag_in_tags(char* tags, char* tag)
{
    bool found = 0;
    while (!found && *tags) {
        tags += *tags == ':';
        char* s = tags;
        for (; *tags && *tags != ':'; ++tags) {  }
        char t = *tags;
        *tags = 0;
        found = strcmp(s, tag) == 0;
        *tags = t;
    }
    return found;
}

static bool is_tag_subset(char* a, char* b)
{
    bool res = 1;

    while (res && *b) {
        b += *b == ':';
        char* s = b;
        for (; *b && *b != ':'; ++b) {  }
        char t = *b;
        *b = 0;
        res = is_tag_in_tags(a, s);
        *b = t;
    }
    return res;
}

static void find_tag(char* tag)
{
    line_t* iter = dirtag_begin();
    line_t* end  = dirtag_end();

    for (; iter != end; ++iter) {
        if (is_tag_subset(iter->tags, tag)) {
            printf("%s%s", iter->file, delimeter);
        }
    }
}

static void remove_file(char* file)
{
    FILE* f = fopen(config_file, "w");
    PANIC_IF(!f, "could not open file %s: '%s'", config_file, strerror(errno));
    line_t* iter = dirtag_begin();
    line_t* end  = dirtag_end();

    for (; iter != end; ++iter) {
        if (strcmp(iter->file, file) != 0) {
            fprintf(f, "%s %s\n", iter->file, iter->tags);
        }
    }

    fclose(f);
}

static void new_file(char* file, char* tags)
{
    FILE* f = fopen(config_file, "a");
    PANIC_IF(!f, "could not open file %s: '%s'", config_file, strerror(errno));

    fprintf(f, "%s all", file);
    if (tags) {
        fprintf(f, ":%s", tags);
    }
    fprintf(f, "\n");
    fclose(f);
}

static flag_t flags[] = {
    {
        .short_identifier = 'h',
        .long_identifier  = "help",
        .description      = "show this page and exit",
        .target           = &flag_help,
        .type             = FLAG_BOOL,
    },
    {
        .short_identifier = 'c',
        .long_identifier  = "config",
        .description      = "config file to use",
        .target           = &config_file,
        .type             = FLAG_STR,
    },
    {
        .short_identifier = 0,
        .long_identifier  = "clear-tag",
        .description      = "clear tags of given file",
        .target           = &flag_clear_tag,
        .type             = FLAG_BOOL,
    },
    {
        .short_identifier = 'a',
        .long_identifier  = "add-tag",
        .description      = "add a tags to given file",
        .target           = &flag_add_tag,
        .type             = FLAG_BOOL,
    },
    {
        .short_identifier = 'f',
        .long_identifier  = "find-tag",
        .description      = "lists all files with given tag",
        .target           = &flag_find,
        .type             = FLAG_BOOL,
    },
    {
        .short_identifier = 'r',
        .long_identifier  = "remove-file",
        .description      = "removes given file from db",
        .target           = &flag_rm_file,
        .type             = FLAG_BOOL,
    },
    {
        .short_identifier = 'd',
        .long_identifier  = "delimeter",
        .description      = "delimeter to use",
        .target           = &delimeter,
        .type             = FLAG_STR,
    },
    {
        .short_identifier = 'n',
        .long_identifier  = "new-file",
        .description      = "adds new file to db",
        .target           = &flag_new_file,
        .type             = FLAG_BOOL,
    },
};

static uint32_t flags_len = sizeof(flags) / sizeof(*flags);


int main(int argc, char** argv)
{
    // flag stuff
    {
        int err = flag_parse(argc, argv, flags, flags_len, &argc, &argv);
        if (err) {
            fprintf(stderr, "Error: while flag parsing: %s: %s", flag_error_format(err), *flag_error_position());
            exit(1);
        }
        if (flag_help) {
            flag_print_usage(stdout, "dirtag - file tagging tool", flags, flags_len);
            exit(1);
        }
    }

    load_config();

    if (flag_clear_tag) {
        clear_tag(argv[1]);
    } else if (flag_add_tag) {
        add_tag(argv[1], argv[2]);
    } else if (flag_find) {
        find_tag(argv[1]);
    } else if (flag_rm_file) {
        remove_file(argv[1]);
    } else if (flag_new_file) {
        new_file(argv[1], argv[2]);
    }

    deinit();
    return 0;
}
