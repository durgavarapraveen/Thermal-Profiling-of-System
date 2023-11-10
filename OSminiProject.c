#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <curl/curl.h>
#include <curl/curl.h>

#define MAX_COMMAND 256
#define MAX_NOTIFICATION_COMMAND 512

#define EMAIL_SUBJECT "CPU Temperature Alert"

// Set the temperature threshold in Celsius
#define THRESHOLD 43

struct upload_status {
    const char *data;
    size_t bytes_read;
};

static size_t payload_source(char *ptr, size_t size, size_t nmemb, void *userp) {
    struct upload_status *upload_ctx = (struct upload_status *)userp;
    size_t room = size * nmemb;

    if (upload_ctx->bytes_read < strlen(upload_ctx->data)) {
        size_t len = strlen(upload_ctx->data + upload_ctx->bytes_read);
        if (room < len)
            len = room;
        memcpy(ptr, upload_ctx->data + upload_ctx->bytes_read, len);
        upload_ctx->bytes_read += len;

        return len;
    }

    return 0;
}

void sendEmail(const char *subject, const char *body) {
    CURL *curl;
    CURLcode res = CURLE_OK;
    struct upload_status upload_ctx;
    char email_content[1024];

    // Create the email content using sprintf
    snprintf(email_content, sizeof(email_content), "Subject: %s\r\n\r\n%s", subject, body);

    upload_ctx.data = email_content;
    upload_ctx.bytes_read = 0;

    curl = curl_easy_init();
    // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    if (curl) {
        struct curl_slist *recipients = NULL;
        recipients = curl_slist_append(recipients, "geda.1@iitj.ac.in");
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, "akarshqwerty3@gmail.com");
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        curl_easy_setopt(curl, CURLOPT_URL, "smtps://smtp.gmail.com");
        curl_easy_setopt(curl, CURLOPT_PORT, 465L);
        curl_easy_setopt(curl, CURLOPT_USERNAME, "akarshqwerty3@gmail.com");
        curl_easy_setopt(curl, CURLOPT_PASSWORD, "ybajmqdqurzqetce");
        
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: text/plain");

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
        curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_INFILESIZE, 1L);
        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
    }
}


// Function to read CPU temperature
float readCPUTemperature() {
    char buffer[MAX_COMMAND];
    FILE *temperatureFile = popen("sensors | grep 'Core 0'", "r");

    if (temperatureFile == NULL) {
        perror("popen");
        exit(1);
    }

    float temperature;
    if (fgets(buffer, sizeof(buffer), temperatureFile) != NULL) {
        char *token = strtok(buffer, "+");
        token = strtok(NULL, "°C");
        temperature = atof(token);
    } else {
        temperature = -1; // Error reading temperature
    }

    pclose(temperatureFile);
    return temperature;
}

// Function to get process information
void getProcessInfo(char *processInfo) {
    char buffer[MAX_COMMAND];
    FILE *processFile = popen("ps -eo pid,%cpu,%mem,comm,user --sort=-%cpu | awk 'NR==2{print $1,$2,$3,$4,$5}'", "r");

    if (processFile == NULL) {
        perror("popen");
        exit(1);
    }

    if (fgets(buffer, sizeof(buffer), processFile) != NULL) {
        strcpy(processInfo, buffer);
    } else {
        strcpy(processInfo, "N/A");
    }

    pclose(processFile);
}

// Function to terminate a specific process
void terminateProcess(pid_t pid, char *processName) {
    kill(pid, SIGTERM);
    printf("process terminated.\n");
}

// Function to extract process name from process information
void extractProcessName(char *processInfo, char *processName) {
    char *token = strtok(processInfo, " ");
    token = strtok(NULL, " ");
    strcpy(processName, token);
}

// Function to restart a terminated process
void restartProcess(char *processName) {
    char command[MAX_COMMAND];
    sprintf(command, "%s &", processName);
    system(command);
    printf("%s process restarted.\n", processName);
}

// Function to display warning and give the option to terminate a process, put it to sleep, or send a notification
void warnAndAct(char *processInfo, int sleepDuration) {
    char processName[MAX_COMMAND];
    extractProcessName(processInfo, processName);

    printf("WARNING: CPU temperature exceeds threshold!\n");
    printf("Process Info: %s\n", processInfo);

    char choice;
    printf("Do you want to terminate the process, put it to sleep, or send a notification? (t/s/n): ");
    scanf(" %c", &choice);

    if (choice == 't') {
        pid_t pid;
        sscanf(processInfo, "%d", &pid);

        terminateProcess(pid, processName);
    } else if (choice == 's') {
        printf("Enter sleep duration after putting the process to sleep (in seconds): ");
        scanf("%d", &sleepDuration);
        printf("Putting the process to sleep for %d seconds...\n", sleepDuration);
        sleep(sleepDuration);
        printf("Process woke up after sleeping for %d seconds.\n", sleepDuration);
    } else {
        // Send a system notification
    }
}

int main() {
    FILE *logFile = fopen("cpu_temperature_log.txt", "a");
    if (logFile == NULL) {
        perror("fopen");
        return 1;
    }

    char logData[MAX_COMMAND]; 

    while (1) {
        float temperature = readCPUTemperature();
        time_t currentTime;
        struct tm *timeInfo;

        time(&currentTime);
        timeInfo = localtime(&currentTime);
        char timestamp[20];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeInfo);

        // Inside the if (temperature > THRESHOLD) block:
	if (temperature > THRESHOLD) {
	    getProcessInfo(logData);
	    
	    // Limit the message length to prevent truncation
	    char truncatedLogData[MAX_COMMAND];
	    snprintf(truncatedLogData, sizeof(truncatedLogData), "%.100s", logData); // Limit to 100 characters
	    
	    char notificationMessage[MAX_COMMAND];
	    snprintf(notificationMessage, sizeof(notificationMessage), "CPU Temperature exceeds threshold!\n\nProcess Info: %.100s", truncatedLogData);

	    char notificationCommand[MAX_NOTIFICATION_COMMAND];
	    snprintf(notificationCommand, sizeof(notificationCommand), "notify-send 'CPU Temperature Alert' '%s'", notificationMessage);
	    system(notificationCommand);
	    printf("Notification sent.\n");
	    
	    char emailBody[1000];
            snprintf(emailBody, sizeof(emailBody), "CPU Temperature: %.1f°C - Process Info: %s", temperature, logData);
            printf("emailBody: %s\n", emailBody);
            sendEmail(EMAIL_SUBJECT, emailBody);

	    int sleepDuration = 0;
	    warnAndAct(logData, sleepDuration);
	}

        
         else {
            fprintf(logFile, "%s - CPU Temperature: %.1f°C\n", timestamp, temperature);
            printf("%s - CPU Temperature: %.1f°C\n", timestamp, temperature);
        }

        sleep(10); // Check temperature every 10 seconds
    }

    fclose(logFile);
    return 0;
}