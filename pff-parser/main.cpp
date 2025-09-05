//
//  main.cpp
//  pff-parser
//
//  Created by miyako on 2025/09/05.
//

#include "pff-parser.h"

static void usage(void)
{
    fprintf(stderr, "Usage:  pff-parser -r -i in -o out -\n\n");
    fprintf(stderr, "text extractor for ost documents\n\n");
    fprintf(stderr, " -%c path: %s\n", 'i' , "document to parse");
    fprintf(stderr, " -%c path: %s\n", 'o' , "text output (default=stdout)");
    fprintf(stderr, " %c: %s\n", '-' , "use stdin for input");
    fprintf(stderr, " -%c: %s\n", 'r' , "raw text output (default=json)");

    exit(1);
}

extern OPTARG_T optarg;
extern int optind, opterr, optopt;

#ifdef WIN32
OPTARG_T optarg = 0;
int opterr = 1;
int optind = 1;
int optopt = 0;
int getopt(int argc, OPTARG_T *argv, OPTARG_T opts) {

    static int sp = 1;
    register int c;
    register OPTARG_T cp;
    
    if(sp == 1)
        if(optind >= argc ||
             argv[optind][0] != '-' || argv[optind][1] == '\0')
            return(EOF);
        else if(wcscmp(argv[optind], L"--") == NULL) {
            optind++;
            return(EOF);
        }
    optopt = c = argv[optind][sp];
    if(c == ':' || (cp=wcschr(opts, c)) == NULL) {
        ERR(L": illegal option -- ", c);
        if(argv[optind][++sp] == '\0') {
            optind++;
            sp = 1;
        }
        return('?');
    }
    if(*++cp == ':') {
        if(argv[optind][sp+1] != '\0')
            optarg = &argv[optind++][sp+1];
        else if(++optind >= argc) {
            ERR(L": option requires an argument -- ", c);
            sp = 1;
            return('?');
        } else
            optarg = argv[optind++];
        sp = 1;
    } else {
        if(argv[optind][++sp] == '\0') {
            sp = 1;
            optind++;
        }
        optarg = NULL;
    }
    return(c);
}
#define ARGS (OPTARG_T)L"i:o:-rh"
#else
#define ARGS "i:o:-rh"
#endif

struct Document {
    std::string type;
    std::string text;
};

#if defined(_WIN32)
static int create_temp_file_path(std::wstring& path) {
    std::vector<wchar_t>buf(1024);
    if (GetTempPathW((DWORD)buf.size(), buf.data()) == 0) return -1;
    if (GetTempFileNameW(buf.data(), L"ost", 0, buf.data()) == 0) return -1;
    path = std::wstring(buf.data());
    return 0;
}
#else
static int create_temp_file_path(std::string& path) {
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir) tmpdir = "/tmp";
    std::vector<char>buf(1024);
    snprintf(buf.data(), buf.size(), "%sXXXXXX.ost", tmpdir);
    path = std::string(buf.data());
    int fd = mkstemp((char *)path.c_str());
    if (fd == -1) return -1;
    close(fd);
    return 0;
}
#endif

static void document_to_json(Document& document, std::string& text, bool rawText) {
    
    if(rawText){
        text += document.text;
    }else{
        Json::Value documentNode(Json::objectValue);
        documentNode["type"] = document.type;
        documentNode["text"] = document.text;
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        text = Json::writeString(writer, documentNode);
    }
}

int main(int argc, OPTARG_T argv[]) {
        
    const OPTARG_T input_path  = NULL;
    const OPTARG_T output_path = NULL;
    
#if defined(_WIN32)
        std::wstring temp_input_path;
#else
        std::string  temp_input_path;
#endif
    
    std::vector<unsigned char>msg_data(0);

    int ch;
    std::string text;
    bool rawText = false;
    
    while ((ch = getopt(argc, argv, ARGS)) != -1){
        switch (ch){
            case 'i':
                input_path  = optarg;
                break;
            case 'o':
                output_path = optarg;
                break;
            case '-':
            {
                _fseek(stdin, 0, SEEK_END);
                size_t len = (size_t)_ftell(stdin);
                _fseek(stdin, 0, SEEK_SET);
                msg_data.resize(len);
                fread(msg_data.data(), 1, msg_data.size(), stdin);
                if(!create_temp_file_path(temp_input_path)){
                    FILE *f = _fopen(temp_input_path.c_str(), _wb);
                    if(f) {
                        fwrite(msg_data.data(), 1, msg_data.size(), f);
                        fclose(f);
                    }
                }
            }
                break;
            case 'r':
                rawText = true;
                break;
            case 'h':
            default:
                usage();
                break;
        }
    }

    const OPTARG_T filename = NULL;
    
    if(input_path) {
        filename = input_path;
    }else{
        if(!msg_data.size()) {
            usage();
        }
        filename = temp_input_path.c_str();
    }
    
    libpff_file_t *file = NULL;
    libpff_error_t *error = NULL;
    
    Document document;

    if (libpff_file_initialize(&file, &error) == 1) {            
        if (_libpff_file_open(file, filename, LIBPFF_OPEN_READ, &error) == 1) {
            libpff_item_t *root_item = NULL;
            document.type = "ost";
            if (libpff_file_get_root_item(file, &root_item, &error) == 1) {
                size_t body_size = 0;
                    if (libpff_message_get_plain_text_body_size(root_item, &body_size, &error) == 1) {
                        std::vector<uint8_t>buf(body_size + 1);
                        if(libpff_message_get_plain_text_body(root_item, buf.data(), buf.size(), &error) == 1) {
                            document.text = (const char *)buf.data();
                            document_to_json(document, text, rawText);
                        }
                    }
                std::cerr << "Failed to get OST body!" << std::endl;
            }else{
                std::cerr << "Failed to get OST root item!" << std::endl;
            }
        }else{
            std::cerr << "Failed to load OST file!" << std::endl;
        }
        libpff_file_free(&file, &error);
    }
    
    if(temp_input_path.length()) {
        _unlink(temp_input_path.c_str());
    }

    if(!output_path) {
        std::cout << text << std::endl;
    }else{
        FILE *f = _fopen(output_path, _wb);
        if(f) {
            fwrite(text.c_str(), 1, text.length(), f);
            fclose(f);
        }
    }
    
    return 0;
}
