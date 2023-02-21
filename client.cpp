/*
    Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20

    Please include your Name, UIN, and the date below
    Name: Arkeldi Bylyku
    UIN: 830004713
    Date: 09/19/2022
*/
#include "FIFORequestChannel.h"
#include "common.h"

using namespace std;

#define MIN(A, B) ((A) < (B) ? (A) : (B))

void close_channel(FIFORequestChannel *_channel) {
    MESSAGE_TYPE q = QUIT_MSG;
    _channel->cwrite(&q, sizeof(MESSAGE_TYPE));
}

void launch_server(unsigned int buf_size) {
    pid_t pid = fork();
    if (pid == 0) {
        char buf[11];
        sprintf(buf, "%d", buf_size);
        execl("./server", "server", "-m", buf, NULL);
    }
}

int main(int argc, char *argv[]) {
    int opt;
    int p = 1;
    double t = 0.0;
    int e = 1;

    bool entire_file, single_item;
    entire_file = false;
    single_item = false;
    bool new_channel = false;

    string filename = "";
    unsigned int buf_size = MAX_MESSAGE;

    while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
        switch (opt) {
        case 'p':
            p = atoi(optarg);
            break;
        case 't':
            t = atof(optarg);
            single_item = true;
            break;
        case 'e':
            e = atoi(optarg);
            single_item = true;
            break;
        case 'f':
            filename = optarg;
            entire_file = true;
            break;
        case 'm':
            buf_size = atoi(optarg);
            break;
        case 'c':
            new_channel = true;
            break;
        }
    }

    launch_server(buf_size);

    FIFORequestChannel main("control", FIFORequestChannel::CLIENT_SIDE);
    FIFORequestChannel *chan = &main;

    char *buf = new char[buf_size];

    if (new_channel) {
        MESSAGE_TYPE m = NEWCHANNEL_MSG;
        chan->cwrite(&m, sizeof(m));
        chan->cread(buf, buf_size);

        printf("New channel name: %s\n", buf);

        chan = new FIFORequestChannel(buf, FIFORequestChannel::CLIENT_SIDE);
    }

    if (entire_file) {
        // Create "received" directory if not exists
        struct stat st;
        if (stat("received", &st) == -1) {
            if (mkdir("received", 0700) == -1) {
                EXITONERROR("mkdir failed");
            }
        }

        // Create file
        string local_filename = "received/" + filename;
        FILE *fp = fopen(local_filename.c_str(), "w");
        if (fp == NULL) {
            EXITONERROR("fopen failed");
        }

        // Get total file size
        filemsg msg(0, 0);
        // write to buf
        memcpy(buf, &msg, sizeof(msg));
        memcpy(buf + sizeof(msg), filename.c_str(), filename.size() + 1);

        chan->cwrite(buf, sizeof(msg) + filename.size() + 1);
        __int64_t file_size;
        chan->cread(&file_size, sizeof(file_size));

        // Read file in chunks of bufsize
        for (__int64_t i = 0; i < file_size; i += buf_size) {
            unsigned int size_to_read = MIN(buf_size, file_size - i);

            filemsg msg(i, size_to_read);
            // write to buf
            memcpy(buf, &msg, sizeof(msg));
            memcpy(buf + sizeof(msg), filename.c_str(), filename.size() + 1);
            if (chan->cwrite(buf, sizeof(msg) + filename.size() + 1) < 0) {
                EXITONERROR("cwrite failed");
            }

            // Read data
            unsigned int read_size = 0;
            // The OS can partition the read into multiple reads
            while (read_size < size_to_read) {
                int n = chan->cread(buf + read_size, size_to_read - read_size);
                if (n < 0) {
                    EXITONERROR("cread failed");
                }
                read_size += n;
            }

            // Write to file
            if (fwrite(buf, 1, size_to_read, fp) != size_to_read) {
                EXITONERROR("fwrite failed");
            }
        }

        fclose(fp);
    } else if (single_item) {
        // Send a single data point request
        datamsg msg(p, t, e);

        memcpy(buf, &msg, sizeof(datamsg));
        chan->cwrite(buf, sizeof(datamsg));

        double reply;
        chan->cread(&reply, sizeof(double));

        printf("%0.3f,%0.3f\n", t, reply);
    } else {
        // Request first 1000 entries
        // Save them to received/x1.csv

        FILE *fp = fopen("received/x1.csv", "w");
        if (fp == NULL) {
            EXITONERROR("fopen failed");
        }

        for (int i = 0; i < 1000; i++) {
            t = 0.004 * i;
            if (fprintf(fp, "%g", t) < 0) {
                EXITONERROR("fprintf failed");
            }
            for (e = 1; e <= 2; e++) {
                datamsg msg(p, t, e);

                memcpy(buf, &msg, sizeof(datamsg));
                chan->cwrite(buf, sizeof(datamsg));

                double reply;
                chan->cread(&reply, sizeof(double));

                if (fprintf(fp, ",%g", reply) < 0) {
                    EXITONERROR("fprintf failed");
                }
            }
            if (fprintf(fp, "\n") < 0) {
                EXITONERROR("fprintf failed");
            }
        }
    }

    close_channel(chan);
    if (new_channel) {
        close_channel(&main);
        delete chan;
    }

    delete[] buf;

}
