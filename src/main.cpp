#include <iostream>
#include <unistd.h>
#include <string>
#include <vector>
#include <sstream>
#include <signal.h>
#include <cassert>
#include "../include/tree.h"
#include <zmq.hpp>

using namespace std;

const int TIMER = 500;
const int DEFAULT_PORT  = 5050; //?
int n = 2;

bool send_message(zmq::socket_t &socket, const string &message_string) {
    zmq::message_t message(message_string.size());
    memcpy(message.data(), message_string.c_str(), message_string.size());
    return socket.send(message); 
}

string receive_message(zmq::socket_t &socket) {
    zmq::message_t message;
    bool ok = false;
    try {
        ok = socket.recv(&message);
    }
    catch (...) {
        ok = false;
    }
    string recieved_message(static_cast<char*>(message.data()), message.size());
    if (recieved_message.empty() || !ok) {
        return "Root is dead";   
    }
    return recieved_message;
}

void create_node(int id, int port) {
    char* arg0 = strdup("./client");
    char* arg1 = strdup((to_string(id)).c_str());
    char* arg2 = strdup((to_string(port)).c_str());
    char* args[] = {arg0, arg1, arg2, NULL};
    execv("./client", args);
}

string get_port_name(const int port) {
    return "tcp://127.0.0.1:" + to_string(port);
}

bool is_number(string val) {
    try {
        int tmp = stoi(val);
        return true;
    }
    catch(exception& e) {
        cout << "Error: " << e.what() << "\n";
        return false;
    }
}

int main() {
    Tree T;
    string command;
    int child_pid = 0;
    int child_id = 0;
    vector<int> nodes;
    zmq::context_t context(1); // Параметр - количество входных/выходных потоков
    zmq::socket_t main_socket(context, ZMQ_REQ); // REP-сокет (ответ сервера) создается сокет типа ZMQ_REP (ответ сервера).
    cout << "Commands:\n";
    cout << "1. create (id)\n";
    cout << "2. exec (id) (count = n a_1 ... a_n)\n";
    cout << "3. kill (id)\n";
    cout << "4. pingall \n";
    cout << "5. exit\n" << endl;    
    while (true) {
        cin >> command;
        if (command == "create") {
	        n++; 
            size_t node_id = 0;
            string str = "";
            string result = "";
            cin >> str;
            if (!is_number(str)) {
                continue;
            }
            node_id = stoi(str);
            if (child_pid == 0) {
                //зачем это всё??
                main_socket.bind(get_port_name(DEFAULT_PORT + node_id));
                main_socket.setsockopt(ZMQ_RCVTIMEO, n * TIMER); //используется для установки параметра сокета ZeroMQ. В данном случае, это устанавливает таймаут приема (receive timeout) для сокета
		        main_socket.setsockopt(ZMQ_SNDTIMEO, n * TIMER); // это опция сокета, которая устанавливает таймаут отправки (send timeout). Это означает максимальное время ожидания при отправке сообщения сокетом.
	        	
                child_pid = fork();
                
                if (child_pid == -1) {
                    cout << "Unable to create first worker node\n";
                    child_pid = 0;
                    exit(1);
                } else if (child_pid == 0) {
                    create_node(node_id, DEFAULT_PORT + node_id);
                } else {
                    child_id = node_id;
                    main_socket.setsockopt(ZMQ_RCVTIMEO, n * TIMER);
		            main_socket.setsockopt(ZMQ_SNDTIMEO, n * TIMER);
                    send_message(main_socket, "pid");
                    result = receive_message(main_socket);
                }
            } else {
		        main_socket.setsockopt(ZMQ_RCVTIMEO, n * TIMER);
		        main_socket.setsockopt(ZMQ_SNDTIMEO, n * TIMER);
                string msg_s = "create " + to_string(node_id);
                send_message(main_socket, msg_s);
                result = receive_message(main_socket);
            }
            if (result.substr(0, 2) == "Ok") {
                T.push(node_id);
                nodes.push_back(node_id);
            }
            cout << result << "\n";
        } else if (command == "kill") {
            int node_id = 0;
            string str = "";
            cin >> str;
            if (!is_number(str)) {
                continue;
            }
            node_id = stoi(str);
            if (child_pid == 0) {
                cout << "Error: Not found\n";
                continue;
            }
            if (node_id == child_id) {
                kill(child_pid, SIGTERM);
                kill(child_pid, SIGKILL);
                child_id = 0;
                child_pid = 0;
                T.kill(node_id);
                cout << "Ok\n";
                continue;
            }
            string message_string = "kill " + to_string(node_id);
            send_message(main_socket, message_string);
            string recieved_message;
	        recieved_message = receive_message(main_socket);
            if (recieved_message.substr(0, min<int>(recieved_message.size(), 2)) == "Ok") {
                T.kill(node_id);
            }
            cout << recieved_message << "\n";
        }

        else if (command == "exec") { //done
            string id_str = "";
            string count_num = "";
	        string nums = "";
            string rez = "";
            int id = 0;
            cin >> id_str >> count_num;
            int count = stoi(count_num);
            for(int i=0;i<count;i++){
                cin >> nums;
                rez += nums + " ";
            }
            if (!is_number(id_str)) {
                continue;
            }
            id = stoi(id_str);
            string message_string = "exec " + to_string(id) + " " + count_num + " " + rez;
            send_message(main_socket, message_string);
            string recieved_message = receive_message(main_socket);
            cout << recieved_message << "\n";
        }

        else if (command == "pingall") { //pingall!!
            string rez_str = "";
            for(int i = 0; i < nodes.size(); i++){
                int id = nodes[i];
                string message_string = "ping " + to_string(id);
                send_message(main_socket, message_string);
                if(stoi(receive_message(main_socket)) != 0){
                    rez_str += receive_message(main_socket) + ";"; 
                }
            }
            if(rez_str.size() == 0){
                cout << "Ok: -1\n";
            }else{
                rez_str.pop_back();
                cout << "Ok " <<rez_str << "\n";
            }
        }

            else if (command == "exit") {
                int n = system("killall client");
                break; 
            }
    }
    return 0;
}
