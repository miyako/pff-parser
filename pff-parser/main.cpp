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
    fprintf(stderr, "text extractor for ost/pst documents\n\n");
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

struct Message {
    std::string subject;
    std::string text;
    std::string html;
    std::string rtf;
};

struct Folder {
    std::string name;
    std::vector<Folder> folders;
    std::vector<Message> messages;
};

struct Document {
    std::string type;
    std::vector<Folder> folders;
};

#if defined(_WIN32)
static int create_temp_file_path(std::wstring& path) {
    std::vector<wchar_t>buf(1024);
    if (GetTempPathW((DWORD)buf.size(), buf.data()) == 0) return -1;
    if (GetTempFileNameW(buf.data(), L"pff", 0, buf.data()) == 0) return -1;
    path = std::wstring(buf.data());
    return 0;
}
#else
static int create_temp_file_path(std::string& path) {
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir) tmpdir = "/tmp";
    std::vector<char>buf(1024);
    snprintf(buf.data(), buf.size(), "pff%sXXXXXX", tmpdir);
    path = std::string(buf.data());
    int fd = mkstemp((char *)path.c_str());
    if (fd == -1) return -1;
    close(fd);
    return 0;
}
#endif

static void __(Folder& folder, Json::Value& folders){

    Json::Value folderNode(Json::objectValue);
    folderNode["name"] = folder.name;
    
    Json::Value messagesNode(Json::arrayValue);
    for (const auto &message : folder.messages) {
        Json::Value messageNode(Json::objectValue);
        messageNode["subject"] = message.subject;
        messageNode["text"] = message.text;
        messagesNode.append(messageNode);
    }
    folderNode["messages"] = messagesNode;
    
    Json::Value foldersNode(Json::arrayValue);
    for (auto &_folder : folder.folders) {
        __(_folder, foldersNode);
    }
    folderNode["folders"] = foldersNode;
    folders.append(folderNode);
}

static void _(Folder& folder, std::string& text){
    
    for (const auto &message : folder.messages) {
        text += message.subject;
        text += message.text;
    }
    for (auto &_folder : folder.folders) {
        _(_folder, text);
    }
}

static void document_to_json(Document& document, std::string& text, bool rawText) {
    
    if(rawText){
        text = "";
        for (auto &folder : document.folders) {
            _(folder, text);
        }
    }else{
        Json::Value documentNode(Json::objectValue);
        documentNode["type"] = document.type;
                
        Json::Value foldersNode(Json::arrayValue);
        for (auto &folder : document.folders) {
            __(folder, foldersNode);
        }
        documentNode["folders"] = foldersNode;
        
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        text = Json::writeString(writer, documentNode);
    }
}

static void process_folder(Folder& document,
                           libpff_file_t *file,
                           libpff_item_t *folder) {
    
    libpff_error_t *error = NULL;
    int num_messages = 0;
    int num_subfolders = 0;
    if(libpff_folder_get_number_of_sub_messages(folder, &num_messages, &error) == 1){
        if(libpff_folder_get_number_of_sub_folders(folder, &num_subfolders, &error) == 1){
         
            for (int i = 0; i < num_messages; ++i) {
                libpff_item_t *sub_message = NULL;
                if(libpff_folder_get_sub_message(folder, i, &sub_message, &error) == 1){
                    Message message;
                    size_t utf8_string_size = 0;
                    if(libpff_message_get_entry_value_utf8_string_size(sub_message,
                                                                       LIBPFF_ENTRY_TYPE_MESSAGE_SUBJECT,
                                                                       &utf8_string_size, &error) == 1){
                        std::vector<uint8_t>buf(utf8_string_size + 1);
                        if(libpff_message_get_entry_value_utf8_string(sub_message,
                                                                      LIBPFF_ENTRY_TYPE_MESSAGE_SUBJECT,
                                                                       buf.data(), buf.size(), &error) == 1){
                            std::string subject = (const char *)buf.data();
                            subject.erase(std::remove(subject.begin(), subject.end(), '\1'), subject.end());
                            message.subject = subject;
                        }
                    }
                    
                    if(libpff_message_get_entry_value_utf8_string_size(sub_message,
                                                                       LIBPFF_ENTRY_TYPE_MESSAGE_BODY_PLAIN_TEXT,
                                                                       &utf8_string_size, &error) == 1){
                        std::vector<uint8_t>buf(utf8_string_size + 1);
                        if(libpff_message_get_entry_value_utf8_string(sub_message,
                                                                      LIBPFF_ENTRY_TYPE_MESSAGE_BODY_PLAIN_TEXT,
                                                                       buf.data(), buf.size(), &error) == 1){
                            message.text = (const char *)buf.data();
                        }
                    }
                    
                    
                    /*
                    if(libpff_message_get_entry_value_utf8_string_size(sub_message,
                                                                       LIBPFF_ENTRY_TYPE_MESSAGE_BODY_HTML,
                                                                       &utf8_string_size, &error) == 1){
                        std::vector<uint8_t>buf(utf8_string_size + 1);
                        if(libpff_message_get_entry_value_utf8_string(sub_message,
                                                                      LIBPFF_ENTRY_TYPE_MESSAGE_BODY_HTML,
                                                                       buf.data(), buf.size(), &error) == 1){
                            document.messages_html.push_back((const char *)buf.data());
                        }
                    }
                    */
                    /*
                    if(libpff_message_get_entry_value_utf8_string_size(sub_message,
                                                                       LIBPFF_ENTRY_TYPE_MESSAGE_BODY_COMPRESSED_RTF,
                                                                       &utf8_string_size, &error) == 1){
                        std::vector<uint8_t>buf(utf8_string_size + 1);
                        if(libpff_message_get_entry_value_utf8_string(sub_message,
                                                                      LIBPFF_ENTRY_TYPE_MESSAGE_BODY_COMPRESSED_RTF,
                                                                       buf.data(), buf.size(), &error) == 1){
                            document.messages_rtf.push_back((const char *)buf.data());
                        }
                    }
                    */
                    document.messages.push_back(message);
                }
            }
            for (int i = 0; i < num_subfolders; ++i) {
                libpff_item_t *sub_folder = NULL;
                if(libpff_folder_get_sub_folder(folder, i, &sub_folder, &error) == 1){
                    size_t utf8_string_size = 0;
                    if(libpff_folder_get_utf8_name_size(sub_folder, &utf8_string_size, &error) == 1){
                        std::vector<uint8_t>buf(utf8_string_size * 1);
                        if(libpff_folder_get_utf8_name(sub_folder, buf.data(), buf.size(), &error) == 1){
                            Folder _folder;
                            _folder.name = (const char *)buf.data();
                            process_folder(_folder, file, sub_folder);
                            document.folders.push_back(_folder);
                        }
                    }
                }
            }
        }
    }
}
static void process_root_folder(Document& document,
                           libpff_file_t *file,
                           libpff_item_t *folder) {
    
    libpff_error_t *error = NULL;
    int num_subfolders = 0;
    if(libpff_folder_get_number_of_sub_folders(folder, &num_subfolders, &error) == 1){
        for (int i = 0; i < num_subfolders; ++i) {
            libpff_item_t *sub_folder = NULL;
            if(libpff_folder_get_sub_folder(folder, i, &sub_folder, &error) == 1){
                size_t utf8_string_size = 0;
                if(libpff_folder_get_utf8_name_size(sub_folder, &utf8_string_size, &error) == 1){
                    std::vector<uint8_t>buf(utf8_string_size * 1);
                    if(libpff_folder_get_utf8_name(sub_folder, buf.data(), buf.size(), &error) == 1){
                        Folder folder;
                        folder.name = (const char *)buf.data();
                        process_folder(folder, file, sub_folder);
                        document.folders.push_back(folder);
                    }
                }
            }
        }
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
            uint8_t content_type = 0;
            if(libpff_file_get_content_type(file, &content_type, &error) == 1){
                switch (content_type) {
                    case LIBPFF_FILE_CONTENT_TYPE_PST:
                        document.type = "pst";
                        break;
                    case LIBPFF_FILE_CONTENT_TYPE_PAB:
                        document.type = "pab";
                        break;
                    case LIBPFF_FILE_CONTENT_TYPE_OST:
                        document.type = "ost";
                        break;
                }
                libpff_item_t *root_folder = NULL;
                if (libpff_file_get_root_folder(file, &root_folder, &error) == 1) {
                    process_root_folder(document, file, root_folder);
                    
                    document_to_json(document, text, rawText);
                }else{
                    std::cerr << "Failed to get PFF root item!" << std::endl;
                }
            }else{
                std::cerr << "Unknown file content type!" << std::endl;
            }
        }else{
            std::cerr << "Failed to load PFF file!" << std::endl;
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
