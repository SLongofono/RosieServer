#include <iostream>
#include <unistd.h>
#include <fstream>
#include <thread>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>


using std::cout;
using std::endl;
using std::string;
using std::thread;
using std::ofstream;
using std::vector;
using namespace cv;

void error(const char *msg);
void open_tcp(int *s, int *c, int p);
void open_udp(int *s, int *c, int p);
void open_bth(int *s, int *c);
void close_inet(int *s, int *c);
void close_bth(int *s, int *c);

void video(int client);
void control(int client);

int main(int argc, char *argv[]) {

    static const int CONNECTION_TYPE_UDP  =     0;
    static const int CONNECTION_TYPE_TCP  =     1;
    static const int CONNECTION_TYPE_BTH  =     2;

    static const int METHOD_VIDEO         =     0;
    static const int METHOD_CONTROL       =     1;
    static const int METHOD_BOTH          =     2;

    int connection_type = CONNECTION_TYPE_TCP;
    int method_type = METHOD_BOTH;
    int sock, client;
    int port = 1234;

    if (argc > 7 || (argc % 2 == 0)) {
        cout << "Usage: ./rc [-ctype CONNECTIONTYPE] [-cmethod CONTROLMETHOD]" << endl;
        cout << "   -ctype tcp:     Use TCP as the connection method." << endl;
        cout << "   -ctype udp:     Use UDP as the connection method." << endl;
        cout << "   -ctype bth:     Use Bluetooth stack as the connection method." << endl;
        cout << "   -cmethod vid:   Just stream video from Rosie." << endl;
        cout << "   -cmethod ctl:   Just stream controls from application to Rosie." << endl;
        cout << "   -cmethod both:  Stream both video and controls for Rosie." << endl;
        cout << "   -port ####:     Use the listed port for TCP/UDP communications." << endl;
    }

    cout << "Welcome to Rosie's Control Board." << endl;

    for (int i = 0; i < argc; i++) {
        if (string(argv[i]) == "-ctype") {
            if (string(argv[i + 1]) == "bth") {
                connection_type = CONNECTION_TYPE_BTH;
                cout << "\tSet to Bluetooth ";
            } else if (string(argv[i + 1]) == "udp") {
                connection_type = CONNECTION_TYPE_UDP;
                cout << "\tSet to UDP ";
            } else {
                cout << "\tSet to TCP ";
            }
        } else if (string(argv[i]) == "-cmethod") {
            if (string(argv[i + 1]) == "vid") {
                method_type = METHOD_VIDEO;
                cout << "with video" << endl;
            } else if (string(argv[i + 1]) == "ctl") {
                method_type = METHOD_CONTROL;
                cout << "with control" << endl;
            } else {
                cout << "with both video and control" << endl;
            }
        }
        if (string(argv[i]) == "-port") {
            port = atoi(argv[i + 1]);
            cout << "\tPort: " << port << endl;
        }
    }
    cout << endl;

    if (connection_type == CONNECTION_TYPE_TCP) {
        open_tcp(&sock, &client, port);
    } else if (connection_type == CONNECTION_TYPE_UDP) {
        open_udp(&sock, &client, port);
    } else {
        open_bth(&sock, &client);
    }

    if (method_type == METHOD_BOTH) {
        thread vid(video, client);
        thread ctl(control, client);

        vid.join();
        ctl.join();
    } else if (method_type == METHOD_CONTROL) {
        thread ctl(control, client);
        ctl.join();
    } else {
        thread vid(video, client);
        vid.join();
    }

    if (connection_type == CONNECTION_TYPE_TCP || connection_type == CONNECTION_TYPE_UDP) {
        close_inet(&sock, &client);
    } else {
        close_bth(&sock, &client);
    }
    return 0;
}

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void open_tcp(int *sock, int *client, int port) {
    cout << "Opening TCP socket" << endl;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    *sock = socket(AF_INET, SOCK_STREAM, 0);

    if (*sock < 0) {
        error("ERROR opening socket");
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    int ret;

    ret = bind(*sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr));

    if (ret < 0) {
        error("ERROR on handling");
    }

    listen(*sock, 5);

    cout << "Listening on port " << port << endl;

    clilen = sizeof(cli_addr);
    *client = accept(*sock,
                     (struct sockaddr *) &cli_addr,
                     &clilen);
    if (*client < 0) {
        error("ERROR on accept");
    } else {
        cout << "Accepted connection." << endl;
    }
}

void open_udp(int *sock, int *client, int port) {

}

void close_inet(int *sock, int *client) {
    close(*client);
    close(*sock);
}

void open_bth(int *sock, int *client) {
    //TODO: Implement bluetooth
}

void close_bth(int *sock, int *client) {
    //TODO: Implement bluetooth
}

void video(int client) {
    cout << "Starting Video " << endl;
    Mat image;
    vector<uchar> buff;
    vector<int> params;

    VideoCapture cap;
    cap.open(0);
    cout << "Log point" << endl;
    if (!cap.isOpened()) {
        error("ERROR opening");
    } else
        cout << "Camera opened" << endl;
    cap.set(CV_CAP_PROP_FRAME_WIDTH, 640);
    cap.set(CV_CAP_PROP_FRAME_HEIGHT, 480);


    int imgSize, conv_size, n;

    while (client >= 0) {
        cout << "In video loop" << endl;
        cap >> image;
        params.push_back(CV_IMWRITE_JPEG_QUALITY);
        params.push_back(70);

        imencode(".jpg", image, buff, params);
        imgSize = image.total() * image.elemSize();

        cout << image.type() << " " << imgSize;
        cout << " " << buff.size() << endl;

        conv_size = htonl(buff.size());
        n = write(client, &conv_size, sizeof(conv_size));

        if (n < 0) {
            error("ERROR writing");
        }

        n = write(client, &buff[0], buff.size());

        if (n < 0) {
            error("ERROR writing");
        }
    }
}

void control(int client) {
    int control, control_bytes;
    int rotation, rotation_bytes;
    int x_pan, y_pan, pan_bytes;
    unsigned char ctl, rot, x, y;

    ofstream output;
    output.open("/dev/ttyACM0");

    while(client >= 0) {
        control = control_bytes = 0;
        rotation = rotation_bytes = 0;
        x_pan = y_pan = 0;
        ctl = rot = '0';

        control_bytes = read(client, &control, sizeof(control));

        if (control_bytes <= 0) {
            error("ERROR reading control");
        }

        rotation_bytes = read(client, &rotation, sizeof(rotation));

        if (rotation_bytes <= 0) {
            error("ERROR reading rotation");
        }

        pan_bytes = read(client, &x_pan, sizeof(x_pan));

        if (pan_bytes <= 0) {
            error("ERROR reading x_pan");
        }

        pan_bytes = read(client, &y_pan, sizeof(y_pan));

        if (pan_bytes <= 0) {
            error("ERROR reading y_pan");
        }

        control = ntohl(control);
        rotation = ntohl(rotation);
        x_pan = ntohl(x_pan);
        y_pan = ntohl(y_pan);

        switch(control) {
            case 21:
                ctl = 36;
                break;
            case 22:
                ctl = 34;
                break;
            case 20:
                ctl = 35;
                break;
            case 19:
                ctl = 33;
                break;
            case 111://break;
                break;
            case 0://stop
                ctl = 32;
                break;
            default:
                break;
        }

        switch(rotation) {
            case 40:
                rot = 38;
                break;
            case 38:
                rot = 39;
                break;
            case 0:
                rot = 37;
                break;
            default:
                break;
        }

        if (x_pan >= -90 && x_pan < 0) {
            x_pan = 0;
        } else if (x_pan < -90 && x_pan >= -180) {
            x_pan = 180;
        }
        if (y_pan >= -90 && y_pan < 0) {
            y_pan = 0;
        } else if (y_pan < -90 && y_pan >= -180) {
            y_pan = 180;
        }
        x_pan = x_pan / 2;

        y_pan = y_pan / 2;
        x = x_pan + 40;
        y = y_pan + 131;

        output << ctl << rot << x << y << endl;
//		cout << ctl << rot << x_pan << y_pan << endl;
    }
    output.close();

}