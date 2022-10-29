#include "commons.h"
pair<string, string> client_socket_listener;
unordered_map<pthread_t, Peer> peer_list;
pthread_t listener_thread;
int client_fd;
bool user_logged_in = false;
string logged_in_user = "";
typedef struct T
{
    Peer peer;
    vector<string> tokens;
} ThreadInfo;
pthread_mutex_t thread_info_maintainer;
class FileInfo
{
public:
    string file_name;
    string file_hash;
    string user_name;
    string cumulative_hash;
    int time_elapsed;
    string group_name;
    string path;
    long long size;
    int last_block_size;
    vector<pair<bool, string>> integrity;
    vector<Peer> peers;
    /**
     * @brief 0-seeding 1-downloading 2-seeding+downloaded 3-Verifying 4- failed
     * 
     */
    int status;
    int blocks;
    pthread_mutex_t file_sync = PTHREAD_MUTEX_INITIALIZER;
    FileInfo(){};
    FileInfo(string path)
    {
        this->file_name = extract_file_name(path);
        this->path = path;
        file_hash = generate_SHA1(file_name);
    }
    FileInfo(string path, int blocks)
    {
        this->file_name = extract_file_name(path);
        this->path = path;
        this->file_hash = generate_SHA1(file_name);
        integrity = vector<pair<bool, string>>(blocks, make_pair(false, ""));
        this->blocks = blocks;
    }
    void file_hash_generation()
    {
        fstream file_descriptor;
        file_descriptor.open(path, ios::in | ios::binary);
        integrity.clear();
        file_descriptor.seekg(0, ios::end);
        size = file_descriptor.tellg();
        file_descriptor.seekg(0, ios::beg);
        last_block_size = size;
        blocks = size / constants_file_block_size;
        if (size % constants_file_block_size)
        {
            blocks += 1;
            last_block_size = size % constants_file_block_size;
        }
        log("Generated hash:");
        for (int i = 1; i < blocks; i++)
        {
            char buffer[constants_file_block_size];
            file_descriptor.read(buffer, constants_file_block_size);
            string gen_hash = generate_SHA1(buffer, constants_file_block_size);
            log("block[" + to_string(i) + "] :" + gen_hash);
            integrity.push_back(make_pair(1, gen_hash));
        }
        char buffer[last_block_size];
        file_descriptor.read(buffer, last_block_size);
        string gen_hash = generate_SHA1(buffer, last_block_size);
        integrity.push_back(make_pair(1, gen_hash));
        log("block[" + to_string(blocks) + "] :" + gen_hash);
        status = 0;
        file_descriptor.close();
    }
    string get_bit_vector()
    {
        string bit_vector = "";
        for (auto i : integrity)
        {
            bit_vector.append(to_string((int)i.first));
        }
        return bit_vector;
    }
    string get_hash(int block_id)
    {
        return integrity[block_id].second;
    }
    void set_hash(int block_id, char *buffer, int size)
    {
        integrity[block_id].first = 1;
        integrity[block_id].second = generate_SHA1(buffer, size);
    }
    int get_integrity()
    {
        int value = 0;
        for (auto b : integrity)
        {
            if (b.first)
                value++;
        }
        return value;
    }
    bool check_integrity()
    {
        if (get_integrity() == blocks)
            return true;
        return false;
    }
    int get_percentage()
    {
        int val = get_integrity();
        float new_val = val * 100;
        int perc = (new_val / blocks);
        return perc;
    }
    bool integrity_reconciliation(FileInfo file)
    {
        if (file.integrity.size() != blocks)
            return false;
        for (int i = 0; i < blocks; i++)
        {
            if (file.integrity[i].second != integrity[i].second || file.integrity[i].first != integrity[i].first)
            {
                log("Block [" + to_string(i + 1) + "]" + "Mismatch hash : [" + file.integrity[i].second + "] [" + integrity[i].second + "]");
                return false;
            }
        }
        return true;
    }
    void generate_cumulative_hash()
    {
        string hash = integrity[0].second;
        for (int i = 1; i < integrity.size(); i++)
        {
            hash.substr(0, SHA_DIGEST_LENGTH);
            hash = hash.append(integrity[i].second);
            hash = hash.substr(0, SHA_DIGEST_LENGTH * 2);
            hash = generate_SHA1(hash);
        }
        cumulative_hash = hash;
    }
    string get_peer_string()
    {
        string peer_list = "";
        for (auto p : peers)
        {
            peer_list.append("[" + p.ip_address + ":" + p.listener_port + "] ");
        }
        return peer_list;
    }
};
/**
 * @brief <file_hash,FileInfo>
 * 
 */
unordered_map<string, FileInfo> hosted_files;
pthread_mutex_t hosted_file_access;
unordered_map<string, FileInfo> download_mirror;
bool file_uploader(vector<string> &tokens)
{
    string path = tokens[1];
    if (!file_query(path))
    {
        highlight_cyan_ln("|| Incorrect file path provided");
        return false;
    }
    string file_name = extract_file_name(path);
    string file_hash = generate_SHA1(file_name);
    if (hosted_files.find(file_hash) != hosted_files.end())
    {
        highlight_cyan_ln("|| File is already uploaded");
        return false;
    }
    FileInfo file = FileInfo(path);
    file.file_hash_generation();
    file.generate_cumulative_hash();
    hosted_files[file.file_hash] = file;
    return true;
}
void send_file_block_hash(int socket_fd, string file_hash)
{
    FileInfo file = hosted_files[file_hash];
    socket_send(socket_fd, to_string(file.blocks)); //blocks
    ack_recieve(socket_fd);
    socket_send(socket_fd, file.file_name); //filename
    ack_recieve(socket_fd);
    socket_send(socket_fd, file.cumulative_hash);
    ack_recieve(socket_fd);
    socket_send(socket_fd, to_string(file.size));
    string group_name = socket_recieve(socket_fd);
    for (auto i : file.integrity) //filehash
    {
        socket_send(socket_fd, i.second);
        ack_recieve(socket_fd);
    }
    hosted_files[file_hash].group_name = group_name;
    ack_send(socket_fd);
}
void file_upload_send(int socket_fd, string file_hash)
{
    send_file_block_hash(socket_fd, file_hash);
    hosted_files[file_hash].user_name = logged_in_user;
    highlight_green_ln(">>" + socket_recieve(socket_fd));
}
void file_upload_verify_send(int socket_fd, string file_hash)
{
    send_file_block_hash(socket_fd, file_hash);
    hosted_files[file_hash].user_name = logged_in_user;
    string reply = ack_recieve(socket_fd);
    if (reply == reply_NACK)
    {
        hosted_files.erase(file_hash);
    }
    ack_send(socket_fd);
    highlight_green_ln(">>" + socket_recieve(socket_fd));
}
void send_file_info(int socket_fd, FileInfo file)
{
    socket_send(socket_fd, file.file_name);
    ack_recieve(socket_fd);
    socket_send(socket_fd, file.file_hash);
    ack_recieve(socket_fd);
    socket_send(socket_fd, to_string(file.blocks));
    ack_recieve(socket_fd);
    socket_send(socket_fd, to_string(file.size));
    ack_recieve(socket_fd);
    socket_send(socket_fd, to_string(file.last_block_size));
    ack_recieve(socket_fd);
    for (int i = 0; i < file.blocks; i++)
    {
        socket_send(socket_fd, to_string((int)file.integrity[i].first));
        ack_recieve(socket_fd);
        socket_send(socket_fd, file.integrity[i].second);
        ack_recieve(socket_fd);
    }
}
FileInfo recieve_file_info(int socket_fd)
{
    FileInfo file = FileInfo();
    file.file_name = socket_recieve(socket_fd);
    ack_send(socket_fd);
    file.file_hash = socket_recieve(socket_fd);
    ack_send(socket_fd);
    file.blocks = stoi(socket_recieve(socket_fd));
    ack_send(socket_fd);
    file.size = stoi(socket_recieve(socket_fd));
    ack_send(socket_fd);
    file.last_block_size = stoi(socket_recieve(socket_fd));
    ack_send(socket_fd);
    for (int i = 0; i < file.blocks; i++)
    {
        bool flag = stoi(socket_recieve(socket_fd));
        ack_send(socket_fd);
        string hash = socket_recieve(socket_fd);
        ack_send(socket_fd);
        file.integrity.push_back(make_pair(flag, hash));
    }
    return file;
}
void show_downloads()
{
    if (!user_logged_in)
    {
        highlight_cyan_ln("|| User is not logged in.");
        return;
    }
    if (!hosted_files.empty())
    {
        bool user_auth = false;
        for (auto f : hosted_files)
        {
            if (f.second.user_name == logged_in_user)
            {
                user_auth = true;
                break;
            }
        }
        if (!user_auth)
        {
            highlight_cyan_ln("|| No downloads available");
            return;
        }
        sync_print_ln("Uploads/Downloads: ");
        sync_print_ln(line);
        for (auto h : hosted_files)
        {
            if (h.second.user_name != logged_in_user)
                continue;
            string ch;
            bool download_flag = false;
            if (h.second.status == 0)
            {
                highlight_green_ln("[S] \t [" + h.second.group_name + "] \t" + h.second.file_name);
            }
            else if (h.second.status == 1)
            {
                string peer_list = h.second.get_peer_string();
                int perc = h.second.get_percentage();
                highlight_yellow_ln("[D] \t [" + h.second.group_name + "] \t" + h.second.file_name + "\t [" + to_string(perc) + "%" + "]\t" + peer_list);
            }
            else if (h.second.status = 2)
            {
                highlight_purple_ln("[C] \t [" + h.second.group_name + "] \t" + h.second.file_name + "\t [" + to_string(h.second.time_elapsed) + "s]");
            }
            else if (h.second.status = 3)
            {
                highlight_blue_ln("[D] \t [" + h.second.group_name + "] \t" + h.second.file_name + "\t [Verifying Integrity...]");
            }
            else if (h.second.status = 4)
            {
                highlight_red_ln("[F] \t [" + h.second.group_name + "] \t" + h.second.file_name + "\t [" + to_string(h.second.time_elapsed) + "s]");
            }
        }
        sync_print_ln(line);
    }
    else
    {
        highlight_cyan_ln("|| No downloads available");
    }
}
/**
 * @brief <command><group_id><file_name><destination_path>
 * 
 * @param tokens 
 * @return true 
 * @return false 
 */
bool file_download_pre_verification(vector<string> &tokens)
{
    string destination_path = tokens[3];
    if (!directory_query(destination_path) || destination_path[destination_path.size() - 1] != '/')
    {
        highlight_cyan_ln("|| Invalid download path");
        return false;
    }
    string file_name = tokens[2];
    string file_hash = generate_SHA1(file_name);
    string complete_path = destination_path.append(file_name);
    if (hosted_files.find(file_hash) != hosted_files.end())
    {
        highlight_cyan_ln("|| File already downloaded");
        return false;
    }
    return true;
}
bool leave_group_file_validator(vector<string> &tokens)
{
    string group_name = tokens[1];
    for (auto file : hosted_files)
    {
        if (file.second.group_name == group_name)
        {
            highlight_cyan_ln("|| File " + file.second.file_name + " is currently being seeded. Cannot leave group. Stop Sharing first");
            return false;
        }
    }
    return true;
}
bool size_validator(vector<string> &tokens)
{
    string transfer_size = tokens[2];
    char *p;
    strtol(transfer_size.c_str(), &p, 10);
    if (*p)
    {
        highlight_cyan_ln("|| Invalid size provided");
        return false;
    }
    else
        return true;
}
bool validator(vector<string> tokens)
{
    if (tokens.size() == 0)
        return false;
    if (tokens[0] == command_create_user && tokens.size() == 3)
        return true;
    else if (tokens[0] == command_login && tokens.size() == 3)
        return true;
    else if (tokens[0] == command_create_group && tokens.size() == 2)
        return true;
    else if (tokens[0] == command_join_group && tokens.size() == 2)
        return true;
    else if (tokens[0] == command_set_transfer_size && tokens.size() == 3)
        return size_validator(tokens);
    else if (tokens[0] == command_logout && tokens.size() == 1)
        return true;
    else if (tokens[0] == command_list_groups && tokens.size() == 1)
        return true;
    else if (tokens[0] == command_list_requests && tokens.size() == 2)
        return true;
    else if (tokens[0] == command_accept_request && tokens.size() == 3)
        return true;
    else if (tokens[0] == command_leave_group && tokens.size() == 2)
        return leave_group_file_validator(tokens);
    else if (tokens[0] == command_upload_file && tokens.size() == 3)
        return file_uploader(tokens);
    else if (tokens[0] == command_list_files && tokens.size() == 2)
        return true;
    else if (tokens[0] == command_stop_share && tokens.size() == 3)
        return true;
    else if (tokens[0] == command_show_downloads && tokens.size() == 1)
    {
        show_downloads();
        return false;
    }
    else if (tokens[0] == command_download_file && tokens.size() == 4)
        return file_download_pre_verification(tokens);
    else if (tokens[0] == command_close && tokens.size() == 1)
        return true;
    else
    {
        highlight_cyan_ln("||Invalid command/parameter");
        return false;
    }
}
void action(vector<string> tokens)
{
    if (tokens.size() == 0)
    {
        throw constants_socket_empty_reply;
    }
    if (tokens[0] == command_print && tokens.size() == 2)
    {
        highlight_green_ln(">>" + tokens[1]);
    }
    else if (tokens[0] == command_login && tokens.size() == 3)
    {
        //highlight_green_ln(">>" + tokens[2]);
        user_logged_in = true;
        logged_in_user = tokens[1];
        sync_print_ln(line);
        highlight_green_ln(">> User:" + logged_in_user + " is logged in");
        sync_print_ln(line);
    }
    else if (tokens[0] == command_logout && tokens.size() == 2)
    {
        //highlight_green_ln(">>" + tokens[1]);
        user_logged_in = false;
        sync_print_ln(line);
        highlight_red_ln(">> User:" + logged_in_user + " is logged out");
        sync_print_ln(line);
        logged_in_user.clear();
    }
    else if (tokens[0] == command_upload_file)
    {
        if (tokens.size() == 3)
        {
            file_upload_send(client_fd, tokens[2]);
        }
        else if (tokens.size() == 4)
        {
            if (!stoi(tokens[1]))
            {
                hosted_files.erase(tokens[2]);
                highlight_green_ln(">>" + tokens[3]);
            }
        }
    }
    else if (tokens[0] == command_upload_verify && tokens.size() == 3)
    {
        file_upload_verify_send(client_fd, tokens[2]);
    }
    else if (tokens[0] == command_download_file && tokens.size() == 6)
    {
        highlight_green_ln(">>" + tokens[5]);
    }
    else if (tokens[0] == command_stop_share && tokens.size() == 3)
    {
        hosted_files.erase(tokens[1]);
        highlight_green_ln(">>" + tokens[2]);
    }
}
void client_startup()
{
    sleep(1);
    if ((client_fd = client_setup(tracker_1)) == -1)
    {
        log("Could not connect to Tracker [" + tracker_1.first + ":" + tracker_1.second + "]");
        exit(EXIT_FAILURE);
    }
    socket_send(client_fd, client_socket_listener.second);
    sync_print_ln(line);
    while (true)
    {
        try
        {
            string command_buffer;
            sync_print("<<");
            fflush(stdout);
            getline(cin, command_buffer);
            vector<string> tokens = input_parser(command_buffer);
            if (!validator(tokens))
            {
                continue;
            }
            else if (tokens[0] == command_close && tokens.size() == 1)
                break;
            string message = pack_message(tokens);
            socket_send(client_fd, message);
            string reply = socket_recieve(client_fd);
            vector<string> reply_tokens = unpack_message(reply);
            action(reply_tokens);
        }
        catch (string error)
        {
            sync_print_ln("Exiting client");
            log(error);
            break;
        }
    }
    close(client_fd);
}
void *write_blocks(void *arg)
{
    ThreadInfo info = *((ThreadInfo *)arg);
    string file_hash = info.tokens[1];
    int start_index = stoi(info.tokens[2]);
    int blocks_write = stoi(info.tokens[3]);
    bool last_block = stoi(info.tokens[4]);
    int file_transfer_size = stoi(info.tokens[5]);
    int socket_fd = client_setup(make_pair(info.peer.ip_address, info.peer.listener_port));
    string command = pack_message(info.tokens);
    socket_send(socket_fd, command);
    ack_recieve(socket_fd);
    fstream file;
    file.open(hosted_files[file_hash].path, ios::binary | ios::out | ios::in);
    file.seekp(start_index * constants_file_block_size, ios::beg); //create buffer here
    char buffer[constants_file_block_size];
    for (int i = start_index; i < start_index + blocks_write; i++)
    {
        ack_send(socket_fd);
        int bytes_to_write = stoi(socket_recieve(socket_fd));
        ack_send(socket_fd);
        bzero(buffer, 0);
        int offset = 0, p_size = 0;
        do
        {
            p_size = read(socket_fd, buffer + offset, file_transfer_size);
            ack_send(socket_fd);
            offset = offset + p_size;
        } while (offset < bytes_to_write);
        file.write(buffer, bytes_to_write);
        hosted_files[file_hash].set_hash(i, buffer, bytes_to_write);
        ack_recieve(socket_fd);
    }
    ack_send(socket_fd);
    file.close();
    close(socket_fd);
}
void *send_blocks(void *arg)
{
    pthread_mutex_lock(&thread_info_maintainer);
    pthread_mutex_unlock(&thread_info_maintainer);
    ThreadInfo info = *((ThreadInfo *)arg);
    try
    {

        string file_hash = info.tokens[1];
        int start_index = stoi(info.tokens[2]);
        int blocks_read = stoi(info.tokens[3]);
        bool last_block = stoi(info.tokens[4]);
        int file_transfer_size = stoi(info.tokens[5]);
        FileInfo seed_file = hosted_files[file_hash];
        fstream file;
        file.open(seed_file.path, ios::binary | ios::in);
        file.seekg(start_index * constants_file_block_size, ios::beg);
        char buffer[constants_file_block_size];
        ack_send(info.peer.socket_fd);
        for (int i = start_index; i < start_index + blocks_read; i++)
        {
            ack_recieve(info.peer.socket_fd);
            bzero(buffer, 0);
            file.read(buffer, constants_file_block_size);
            int bytes_read = file.gcount();
            socket_send(info.peer.socket_fd, to_string(bytes_read));
            ack_recieve(info.peer.socket_fd);
            int offset = 0, p_size = 0;
            do
            {
                p_size = write(info.peer.socket_fd, buffer + offset, file_transfer_size);
                offset = offset + p_size;
                ack_recieve(info.peer.socket_fd);
            } while (offset < bytes_read);
            ack_send(info.peer.socket_fd);
        }
        ack_recieve(info.peer.socket_fd);
        file.close();
    }
    catch (string error)
    {
        log(error);
    }
    peer_list.erase(pthread_self());
}
void *download_start(void *arg)
{
    pthread_mutex_lock(&thread_info_maintainer);
    pthread_mutex_unlock(&thread_info_maintainer);
    ThreadInfo info = *((ThreadInfo *)arg);
    try
    {
        auto start_time = std::chrono::high_resolution_clock::now();
        string file_name = info.tokens[1];
        string file_hash = generate_SHA1(file_name);
        string cumulative_hash = info.tokens[2];
        string file_path = info.tokens[3];
        string group_name = info.tokens[4];
        int blocks = stoi(info.tokens[5]);
        long long size = stoll(info.tokens[6]);
        string user_name = info.tokens[7];
        int file_transfer_size = stoi(info.tokens[8]);
        int number_of_peers = stoi(info.tokens[9]);
        file_path.append(file_name);
        FileInfo target_file = FileInfo(file_path, blocks);
        target_file.size = size;
        target_file.user_name = user_name;
        target_file.group_name = group_name;
        for (int i = 1; i <= number_of_peers; i++)
        {
            pair<string, string> socket = read_socket_input(info.tokens[9 + i]);
            Peer new_peer = Peer();
            new_peer.ip_address = socket.first;
            new_peer.listener_port = socket.second;
            target_file.peers.push_back(new_peer);
        }
        /*
        int socket_fd = client_setup(make_pair(peers[0].ip_address, peers[0].listener_port));
        vector<string> command_tokens = {command_fetch_file_info, file_hash};
        socket_send(socket_fd, pack_message(command_tokens));
        FileInfo to_download = recieve_file_info(socket_fd);
        close(socket_fd);
        */
        hosted_files[file_hash] = target_file;
        create_dummy_file(target_file.path, size);
        int allocation = blocks / number_of_peers;
        pthread_t download_threads[number_of_peers];
        int total = blocks;
        int start = 0;
        hosted_files[file_hash].status = 1;
        for (int i = 0; i < number_of_peers; i++)
        {
            int current_alloc = allocation;
            bool flag_last_block = false;
            if (i == number_of_peers - 1)
            {
                current_alloc = total;
                total = 0;
                flag_last_block = true;
            }
            else
                total -= current_alloc;
            log(to_string(i) + ": Downloading from Peer: " + hosted_files[file_hash].peers[i].ip_address + " " + hosted_files[file_hash].peers[i].listener_port + " from " + to_string(start) + "th block to " + to_string(start + current_alloc) + "th");
            vector<string> command_tokens = {command_send_blocks, file_hash, to_string(start), to_string(current_alloc), to_string((int)flag_last_block), to_string(file_transfer_size)};
            start += current_alloc;
            ThreadInfo *new_info = new ThreadInfo;
            new_info->tokens = command_tokens;
            new_info->peer = hosted_files[file_hash].peers[i];
            pthread_create(&download_threads[i], NULL, write_blocks, new_info);
        }
        for (int i = 0; i < number_of_peers; i++)
        {
            pthread_join(download_threads[i], NULL);
        }
        log("Threads have joined");
        log("Retrieving file integrity hash values from peer");
        hosted_files[file_hash].status = 3;
        int socket_fd = client_setup(make_pair(hosted_files[file_hash].peers[0].ip_address, hosted_files[file_hash].peers[0].listener_port));
        vector<string> command_tokens = {command_fetch_file_info, file_hash};
        socket_send(socket_fd, pack_message(command_tokens));
        FileInfo to_download = recieve_file_info(socket_fd);
        close(socket_fd);
        sleep(1);
        if (!hosted_files[file_hash].integrity_reconciliation(to_download))
        {
            log("Download failed file integrity compromised");
            hosted_files[file_hash].status = 4;
            socket_send(info.peer.socket_fd, reply_download_status_FAILURE);
        }
        else
        {
            log("File:" + target_file.file_name + " has completed download");
            hosted_files[file_hash].status = 2;
            socket_send(info.peer.socket_fd, reply_download_status_SUCCESS);
        }
        auto end_time = std::chrono::high_resolution_clock::now();
        hosted_files[file_hash].time_elapsed = chrono::duration_cast<chrono::seconds>(end_time - start_time).count();
    }
    catch (string error)
    {
        log(error);
    }
    peer_list.erase(pthread_self());
}
void *fetch_file_info(void *arg)
{
    pthread_mutex_lock(&thread_info_maintainer);
    pthread_mutex_unlock(&thread_info_maintainer);
    ThreadInfo info = *((ThreadInfo *)arg);
    try
    {
        string file_hash = info.tokens[1];
        FileInfo file = hosted_files[file_hash];
        send_file_info(info.peer.socket_fd, file);
    }
    catch (string error)
    {
        log(error);
    }
    peer_list.erase(pthread_self());
}
void process(vector<string> tokens, Peer peer)
{
    ThreadInfo *info = new ThreadInfo;
    info->peer = peer;
    info->tokens = tokens;
    pthread_t worker_thread;
    pthread_mutex_lock(&thread_info_maintainer);
    if (tokens[0] == command_download_init)
    {
        pthread_create(&worker_thread, NULL, download_start, info);
    }
    else if (tokens[0] == command_send_blocks)
    {
        pthread_create(&worker_thread, NULL, send_blocks, info);
    }
    else if (tokens[0] == command_fetch_file_info)
    {
        pthread_create(&worker_thread, NULL, fetch_file_info, info);
    }
    log("Connection to peer established :" + info->peer.ip_address + " " + info->peer.port);
    peer_list[worker_thread] = peer;
    pthread_mutex_unlock(&thread_info_maintainer);
}
void *listener_startup(void *)
{
    int listener_fd = server_setup(client_socket_listener);
    while (true)
    {
        struct sockaddr_storage peer_address;
        socklen_t peer_addr_size = sizeof(peer_address);
        Peer new_peer = Peer();
        new_peer.socket_fd = accept(listener_fd, (struct sockaddr *)&peer_address, &peer_addr_size);
        if (new_peer.socket_fd == -1)
        {
            log("Unable to connect");
            continue;
        }
        struct sockaddr_in peer_address_cast = *((struct sockaddr_in *)&peer_address);
        new_peer.ip_address = inet_ntoa(peer_address_cast.sin_addr);
        new_peer.port = to_string(ntohs(peer_address_cast.sin_port));
        int *fd = new int;
        *fd = new_peer.socket_fd;
        string command_message = socket_recieve(new_peer.socket_fd);
        vector<string> command_message_tokens = unpack_message(command_message);
        process(command_message_tokens, new_peer);
    }
    close(listener_fd);
}

int main(int argc, char *argv[])
{
    logging_level = 2;
    if (argc != 3 && argc != 4)
    {
        exit(1);
    }
    else if (argc == 4)
    {
        logging_level = atoi(argv[3]);
    }
    string file_path(argv[2]);
    string socket_input(argv[1]);
    set_log_file("client_log_file.log");
    read_tracker_file(file_path);
    client_socket_listener = read_socket_input(socket_input);
    sync_print_ln("[PEER: IP: " + client_socket_listener.first + " LISTENING PORT: " + client_socket_listener.second + " ]");
    pthread_create(&listener_thread, NULL, listener_startup, NULL);
    client_startup();
    return 0;
}