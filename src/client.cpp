#include <iostream>
#include <unistd.h>
#include <string>
#include <sstream>
#include <exception>
#include <signal.h>
#include <zmq.hpp>

using namespace std;

const int TIMER = 500;
const int DEFAULT_PORT  = 5050;
int n = 2;

bool message_send(zmq::socket_t &socket, const string &message_string) {
    zmq::message_t message(message_string.size());
    memcpy(message.data(), message_string.c_str(), message_string.size());
    return socket.send(message);
}

string receive_message(zmq::socket_t &socket) {
    zmq::message_t message;
    bool ok = false; 
    try {
        ok = socket.recv(&message); //метод recv() используется для приема сообщения через сокет.
    }
    catch (...) {
        ok = false;
    }
    string recieved_message(static_cast<char*>(message.data()), message.size()); //.data - возвращает указатель на начало буфера данных сообщения.
    if (recieved_message.empty() || !ok) {
        return "";
    }
    return recieved_message;
}

void create_node(int id, int port) {
    char* arg0 = strdup("./client");
    char* arg1 = strdup((to_string(id)).c_str()); //метод .c_str() используется для получения указателя на строку 
    char* arg2 = strdup((to_string(port)).c_str());
    char* args[] = {arg0, arg1, arg2, NULL};
    execv("./client", args);
}

string get_port_name(const int port) {
    return "tcp://127.0.0.1:" + to_string(port);
}

void norm_kill(zmq::socket_t& main_socket, zmq::socket_t& socket, int& delete_id, int& id, int& pid, string& request_string) {
    if (id == 0) {
        message_send(main_socket, "Error: Not found");
    } 
    else if (id == delete_id) {
        message_send(socket, "kill_children");
        receive_message(socket);
        kill(pid, SIGTERM);
        kill(pid, SIGKILL);
        id = 0;
        pid = 0;
        message_send(main_socket, "Ok");
    } 
    else {
        message_send(socket, request_string);
        message_send(main_socket, receive_message(socket));
    }
}

void norm_create(zmq::socket_t& main_socket, zmq::socket_t& socket, int& create_id, int& id, int& pid) {
    if (pid == -1) {
        message_send(main_socket, "Error: Cannot fork");
        pid = 0;
    } 
    else if (pid == 0) {
        create_node(create_id,DEFAULT_PORT + create_id);
    } 
    else {
        id = create_id;
        message_send(socket, "pid");
        message_send(main_socket, receive_message(socket));
    }
}

void noem_exec(zmq::socket_t& main_socket, zmq::socket_t& socket,  int& id, int& pid, string& request_string) {
    if (pid == 0) {
        string receive_message = "Error:" + to_string(id);
        receive_message += ": Not found";
        message_send(main_socket, receive_message);
    } 
    else {
        message_send(socket, request_string);
        string str = receive_message(socket);
        if (str == "") str = "Error: Node is unavailable";
        message_send(main_socket, str);
    }
}

void exec(istringstream& command_stream, zmq::socket_t& main_socket, zmq::socket_t& left_socket, 
            zmq::socket_t& right_socket, int& left_pid, int& right_pid, int& id, string& request_string) {
    string count_nums, nums;
    int exec_id;
    command_stream >> exec_id;
    if (exec_id == id) {
        command_stream >> count_nums;
        string receive_message = "";
        int sum = 0;
        int count;

        while (command_stream >> count) {
            sum += count;
        }
        receive_message = to_string(sum);
        message_send(main_socket, receive_message);
    } else if (exec_id < id) {
        noem_exec(main_socket, left_socket, exec_id, left_pid, request_string);
    } else {
        noem_exec(main_socket, right_socket, exec_id, right_pid, request_string);
    }
}


void real_ping(zmq::socket_t& main_socket, zmq::socket_t& socket,  int& id, int& pid, string& request_string) {
    if (pid != 0) {
        message_send(socket, request_string);
        string str = receive_message(socket);
        if (str == "") str = "0";
        message_send(main_socket, str);
    }
    else {
        string receive_message = "Error:" + to_string(id);
        receive_message += ": Not found";
        message_send(main_socket, receive_message);
    }
}

void ping(istringstream& command_stream, zmq::socket_t& main_socket, zmq::socket_t& left_socket,
            zmq::socket_t& right_socket, int& left_pid, int& right_pid, int& id, string& request_string) {
    int ping_id;
    string receive_message;
    command_stream >> ping_id;
    if (ping_id == id) {
        receive_message = "0";
        message_send(main_socket, receive_message);
    } else if (ping_id < id) {
        real_ping(main_socket, left_socket, ping_id, left_pid, request_string);
    }
    else {
        real_ping(main_socket, right_socket, ping_id, right_pid, request_string);
    }
}




void kill_children(zmq::socket_t& main_socket, zmq::socket_t& left_socket, zmq::socket_t& right_socket, int& left_pid, int& right_pid) {
    if (left_pid == 0 && right_pid == 0) {
        message_send(main_socket, "Ok");
    } else {
        if (left_pid != 0) {
            message_send(left_socket, "kill_children");
            receive_message(left_socket);
            kill(left_pid,SIGTERM);
            kill(left_pid,SIGKILL);
        }
        if (right_pid != 0) {
            message_send(right_socket, "kill_children");
            receive_message(right_socket);
            kill(right_pid,SIGTERM);
            kill(right_pid,SIGKILL);
        }
        message_send(main_socket, "Ok");
    }
}

int main(int argc, char** argv) {
    int id = stoi(argv[1]);
    int parent_port = stoi(argv[2]);
    zmq::context_t context(3);
    zmq::socket_t main_socket(context, ZMQ_REP);
    main_socket.connect(get_port_name(parent_port));
    main_socket.setsockopt(ZMQ_RCVTIMEO, TIMER);
    main_socket.setsockopt(ZMQ_SNDTIMEO, TIMER);
    int left_pid = 0;
    int right_pid = 0;
    int left_id = 0;
    int right_id = 0;
    zmq::socket_t left_socket(context, ZMQ_REQ);
    zmq::socket_t right_socket(context, ZMQ_REQ);
    while(true) {
        string request_string = receive_message(main_socket);
        istringstream command_stream(request_string);
        string command;
        command_stream >> command;

        if (command == "id") {
            string parent_string = "Ok:" + to_string(id);
            message_send(main_socket, parent_string);
        } else if (command == "pid") {
            string parent_string = "Ok:" + to_string(getpid());
            message_send(main_socket, parent_string);
        } else if (command == "create") {
            int create_id;
            command_stream >> create_id;
            if (create_id == id) {
                string message_string = "Error: Already exists";
                message_send(main_socket, message_string);
            } else if (create_id < id) {
                if (left_pid == 0) {
                    left_socket.bind(get_port_name(DEFAULT_PORT + create_id));
                    left_socket.setsockopt(ZMQ_RCVTIMEO, n * TIMER);
	                left_socket.setsockopt(ZMQ_SNDTIMEO, n * TIMER);
                    left_pid = fork();
                    norm_create(main_socket, left_socket, create_id, left_id, left_pid);
                } else {
                    message_send(left_socket, request_string);
                    string str = receive_message(left_socket);
                    if (str == "") {
                        left_socket.bind(get_port_name(DEFAULT_PORT + create_id));
                        left_socket.setsockopt(ZMQ_RCVTIMEO, n * TIMER);
                        left_socket.setsockopt(ZMQ_SNDTIMEO, n * TIMER);
                        left_pid = fork();
                        norm_create(main_socket, left_socket, create_id, left_id, left_pid);
                    } else {
                        message_send(main_socket, str);
                        n++;
                        left_socket.setsockopt(ZMQ_RCVTIMEO, n * TIMER);
                        left_socket.setsockopt(ZMQ_SNDTIMEO, n * TIMER);
                    }
                }
            } else {
                if (right_pid == 0) {
                    right_socket.bind(get_port_name(DEFAULT_PORT + create_id));
                    right_socket.setsockopt(ZMQ_RCVTIMEO, n * TIMER);
	                right_socket.setsockopt(ZMQ_SNDTIMEO, n * TIMER);
                    right_pid = fork();
                    norm_create(main_socket, right_socket, create_id, right_id, right_pid);
                } else {
                    message_send(right_socket, request_string);
                    string str = receive_message(right_socket);
                    if (str == "") {
                        right_socket.bind(get_port_name(DEFAULT_PORT + create_id));
                        right_socket.setsockopt(ZMQ_RCVTIMEO, n * TIMER);
                        right_socket.setsockopt(ZMQ_SNDTIMEO, n * TIMER);
                        right_pid = fork();
                        norm_create(main_socket, right_socket, create_id, right_id, right_pid);
                    } else {
                        message_send(main_socket, str);
                        n++;
                        right_socket.setsockopt(ZMQ_RCVTIMEO, n * TIMER);
                        right_socket.setsockopt(ZMQ_SNDTIMEO, n * TIMER);
                    }
                }
            }
        } else if (command == "kill") {
            int delete_id;
            command_stream >> delete_id;
            if (delete_id < id) {
                norm_kill(main_socket, left_socket, delete_id, left_id, left_pid, request_string);
            } else {
                norm_kill(main_socket, right_socket, delete_id, right_id, right_pid, request_string);
            }
        } else if (command == "exec") {
            exec(command_stream, main_socket, left_socket, right_socket, left_pid, right_pid, id, request_string);
        } else if (command == "ping") {
	        ping(command_stream, main_socket, left_socket, right_socket, left_pid, right_pid, id, request_string);
	    } else if (command == "kill_children") {
            kill_children(main_socket, left_socket, right_socket, left_pid, right_pid); 
        }
        if (parent_port == 0) {
            break;
        }
    }
    return 0;
}
