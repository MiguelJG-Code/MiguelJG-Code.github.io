#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include <zint.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#include <shellapi.h>
#include <direct.h>
#endif

#include <curl/curl.h>

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#endif

#define EMAIL_SENDER_NAME "FOURSIGHT CAR RENTALS"
#define EMAIL_SENDER_ADDRESS "jazdigamon12@gmail.com"
#define EMAIL_APP_PASSWORD "zvse hgdn vmna qzrz"

#define MAX_CARS 400
#define MAX_CUSTOMERS 1000
#define MAX_RENTALS 2000
#define MAX_USERS 50

#define BARCODE_LEN 64
#define PLATE_LEN 24
#define MODEL_LEN 80
#define NAME_LEN 100
#define LICENSE_LEN 40
#define CONTACT_LEN 40
#define USERNAME_LEN 40
#define PASSWORD_LEN 64
#define EMAIL_LEN 160
#define FILENAME_LEN 260

/* Text DB filenames */
#define DB_INVENTORY_FILE "inventory.txt"
#define DB_CUSTOMERS_FILE "customers.txt"
#define DB_RENTALS_FILE "rentals.txt"
#define DB_USERS_FILE "users.txt"
#define RECEIPTS_DIR "receipts"
#define BARCODES_DIR "barcodes"

#ifdef _WIN32
#define CLEAR_CMD "cls"
#else
#define CLEAR_CMD "clear"
#endif

#ifdef _WIN32
#define STRICMP _stricmp
#else
#define STRICMP strcasecmp
#endif

/* Data structures */
typedef struct
{
    char barcode[BARCODE_LEN];
    char plateNumber[PLATE_LEN];
    char model[MODEL_LEN];
    double pricePerDay;
    int available;
    int totalRentedCount;
    int id;
} Car;

typedef struct
{
    int id;
    char name[NAME_LEN];
    char licenseNo[LICENSE_LEN];
    char contact[CONTACT_LEN];
    char email[EMAIL_LEN];
} Customer;

typedef struct
{
    int id;
    char barcode[BARCODE_LEN];
    char plateNumber[PLATE_LEN];
    char model[MODEL_LEN];
    char customerName[NAME_LEN];
    char licenseNo[LICENSE_LEN];
    int quantity;
    int days;
    double totalAmount;
    time_t rentTime;
    int returned;
    time_t returnTime;
} RentalRecord;

typedef struct
{
    char username[USERNAME_LEN];
    char password[PASSWORD_LEN];
    int isAdmin;
    int id;
} User;

/* Rental Item for bulk transactions */
typedef struct
{
    int carIndex;
    int quantity;
    int days;
    double subtotal;
} RentalItem;

/* Global arrays */
Car inventory[MAX_CARS];
int carCount = 0;
int nextCarId = 1001;

Customer customers[MAX_CUSTOMERS];
int customerCount = 0;
int nextCustomerId = 1;

RentalRecord rentals[MAX_RENTALS];
int rentalCount = 0;
int nextRentalId = 1;

User users[MAX_USERS];
int userCount = 0;
int nextUserId = 1;

int terminalSupportsColor = 0;

/* Color codes */
enum
{
    CLR_RESET = 0,
    CLR_RED,
    CLR_GREEN,
    CLR_YELLOW,
    CLR_BLUE,
    CLR_MAGENTA,
    CLR_CYAN,
    CLR_WHITE
};

/* FUNCTION PROTOTYPES */
/* Core system functions */
void enableTerminalColors();
void setColor(int color);
void resetColor();
void safeInput(char *buf, int maxlen, const char *prompt);
void flushInput();
void pressEnterToContinue();
void clearScreen();
int fileExists(const char *filename);
int createDirectoriesIfNeeded(const char *path);
void ensureReceiptsDir();
void printBanner();
int showLoginHeader();

/* Database functions */
void saveInventory();
void loadInventory();
void saveCustomers();
void loadCustomers();
void saveRentals();
void loadRentals();
void saveUsers();
void loadUsers();

/* Default data */
void addDefaultCars();
void addDefaultUsers();
void initializeDefaultDataIfEmpty();
void initializeSystem();

/* Search functions */
int findCarByBarcode(const char *barcode);
int findCarByPlate(const char *plate);
int findCustomerByLicense(const char *license);
int findCustomerByName(const char *name);
int findUserByUsername(const char *username);

/* Email functions */
size_t curl_payload_read(void *ptr, size_t size, size_t nmemb, void *userp);
int sendEmailLibCurl(const char *toAddress, const char *subject, const char *body);

/* Barcode functions */
void printAsciiBarcode(const char *code, FILE *out);
void generateBarcodePNG(const char *code, const char *outputFile);
void generateRentalBarcodePNG(int rentalId, const char *barcodeData);  /* NEW */
void autoOpenFile(const char *filename);

/* Receipt functions */
void autoPrintReceipt(const char *filename);
void printReceiptFromRental(const RentalRecord *r);
int saveSimpleReceipt(int rentalId, const char *customerName, const char *license,
                      const char *model, const char *plate, const char *barcode,
                      int qty, int days, double total);
void saveBulkReceipt(int transactionId,                    /* NEW */
                     const char *custName,
                     const char *custLicense,
                     const char *custContact,
                     const char *custEmail,
                     const char *cashierName,
                     RentalItem items[],
                     int itemCount,
                     double total,
                     double cash,
                     double change,
                     int firstRentalId,
                     int lastRentalId);
void reprintReceiptMenu();

/* Login and main interface */
int login(char *loggedUser, int *isAdmin);
void mainDashboard(const char *loggedUser, int isAdmin);

/* Inventory management */
void showInventoryListDetailed();
void viewInventoryInteractive();
void showQuickInventory();

/* Customer management */
int selectCustomerMenu();
void listCustomers();
void addCustomer();
void editCustomer();
void deleteCustomer();
void searchCustomer();
void customerMenu();

/* SIMPLIFIED RENTAL SYSTEM - as mentor requested */
void rentCarMenu(const char *cashierName);
int selectRentalByMenu();
void returnCar();

/* Payment/Billing */
void paymentBillingMenu();

/* Reports */
void reportInventory();
void reportCustomers();
void reportRentals();
void reportsMenu();

/* Settings */
void changePassword(const char *currentUser);
void settingsMenu(const char *currentUser);

/* Admin user management */
void userManagementMenu(const char *currentUser);
void addUserMenu();
void viewUsersMenu();
void deleteUserMenu();
void changeUserPasswordMenu();
void toggleAdminMenu();
int selectUserMenu();

/* FUNCTION IMPLEMENTATIONS */

void enableTerminalColors()
{
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
    {
        terminalSupportsColor = 0;
        return;
    }
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode))
    {
        terminalSupportsColor = 0;
        return;
    }
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (SetConsoleMode(hOut, dwMode))
        terminalSupportsColor = 1;
    else
        terminalSupportsColor = 0;
#else
    terminalSupportsColor = 1;
#endif
}

void setColor(int color)
{
    if (!terminalSupportsColor)
        return;
    switch (color)
    {
    case CLR_RED:
        printf("\x1b[31m");
        break;
    case CLR_GREEN:
        printf("\x1b[32m");
        break;
    case CLR_YELLOW:
        printf("\x1b[33m");
        break;
    case CLR_BLUE:
        printf("\x1b[34m");
        break;
    case CLR_MAGENTA:
        printf("\x1b[35m");
        break;
    case CLR_CYAN:
        printf("\x1b[36m");
        break;
    case CLR_WHITE:
        printf("\x1b[37m");
        break;
    default:
        printf("\x1b[0m");
        break;
    }
}

void resetColor()
{
    if (!terminalSupportsColor)
        return;
    printf("\x1b[0m");
}

void safeInput(char *buf, int maxlen, const char *prompt)
{
    if (prompt)
        printf("%s", prompt);
    if (!fgets(buf, maxlen, stdin))
    {
        buf[0] = 0;
        return;
    }
    buf[strcspn(buf, "\n")] = 0;
}

void flushInput()
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
}

void pressEnterToContinue()
{
    printf("\nPress Enter to continue...");
    fflush(stdout);
    getchar();
}

void clearScreen()
{
    system(CLEAR_CMD);
}

int fileExists(const char *filename)
{
    struct stat st;
    return (stat(filename, &st) == 0);
}

int createDirectoriesIfNeeded(const char *path)
{
#ifdef _WIN32
    return (_mkdir(path) == 0 || errno == EEXIST);
#else
    return (mkdir(path, 0755) == 0 || errno == EEXIST);
#endif
}

void ensureReceiptsDir()
{
    createDirectoriesIfNeeded(RECEIPTS_DIR);
}

void printBanner()
{
    setColor(CLR_CYAN);
    printf("========================================\n");
    printf("FOURSIGHT CAR RENTALS MANAGEMENT SYSTEM\n");
    printf("========================================\n");
    resetColor();
}

int showLoginHeader()
{
    int choice = 0;
    clearScreen();

    printf("========================================\n");
    printf("FOURSIGHT CAR RENTAL MANAGEMENT SYSTEM\n");
    printf("========================================\n");

    while (1)
    {
        printf("Login as:\n");
        printf("1. Admin\n");
        printf("2. Cashier\n");
        printf("Choose option (1-2): ");
        if (scanf("%d", &choice) != 1)
        {
            flushInput();
            printf("Invalid input. Please enter 1 or 2.\n");
            continue;
        }
        flushInput();
        if (choice == 1 || choice == 2)
            return choice;
        printf("Please choose 1 for Admin or 2 for Cashier.\n");
    }
    return 0;
}

/* DATABASE FUNCTIONS */

void saveInventory()
{
    FILE *fp = fopen(DB_INVENTORY_FILE, "w");
    if (!fp)
        return;
    fprintf(fp, "# Inventory file\n");
    fprintf(fp, "CARCOUNT:%d\nNEXTID:%d\n", carCount, nextCarId);
    for (int i = 0; i < carCount; i++)
    {
        Car *c = &inventory[i];
        fprintf(fp, "ID|%d|BARCODE|%s|PLATE|%s|MODEL|%s|PRICE|%.2f|AVAIL|%d|RENTED|%d\n",
                c->id, c->barcode, c->plateNumber, c->model, c->pricePerDay, c->available, c->totalRentedCount);
    }
    fclose(fp);
}

void loadInventory()
{
    carCount = 0;
    nextCarId = 1001;
    FILE *fp = fopen(DB_INVENTORY_FILE, "r");
    if (!fp)
    {
        carCount = 0;
        return;
    }
    char line[1024];
    while (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, "NEXTID:", 7) == 0)
        {
            nextCarId = atoi(line + 7);
            if (nextCarId < 1001)
                nextCarId = 1001;
        }
        else if (strncmp(line, "ID|", 3) == 0)
        {
            Car c = {0};
            char tmp[1024];
            strncpy(tmp, line, sizeof(tmp) - 1);
            char *tok = strtok(tmp, "|");
            tok = strtok(NULL, "|");
            if (tok)
                c.id = atoi(tok);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                strncpy(c.barcode, tok, BARCODE_LEN - 1);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                strncpy(c.plateNumber, tok, PLATE_LEN - 1);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                strncpy(c.model, tok, MODEL_LEN - 1);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                c.pricePerDay = atof(tok);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                c.available = atoi(tok);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                c.totalRentedCount = atoi(tok);
            if (c.id >= nextCarId)
                nextCarId = c.id + 1;
            if (carCount < MAX_CARS)
                inventory[carCount++] = c;
        }
    }
    fclose(fp);
}

void saveCustomers()
{
    FILE *fp = fopen(DB_CUSTOMERS_FILE, "w");
    if (!fp)
        return;
    fprintf(fp, "# Customers file\n");
    fprintf(fp, "CUSTOMERS:%d\nNEXTID:%d\n", customerCount, nextCustomerId);
    for (int i = 0; i < customerCount; i++)
    {
        Customer *c = &customers[i];
        fprintf(fp, "ID|%d|NAME|%s|LICENSE|%s|CONTACT|%s|EMAIL|%s\n",
                c->id, c->name, c->licenseNo, c->contact, c->email);
    }
    fclose(fp);
}

void loadCustomers()
{
    customerCount = 0;
    nextCustomerId = 1;
    FILE *fp = fopen(DB_CUSTOMERS_FILE, "r");
    if (!fp)
        return;
    char line[1024];
    while (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, "NEXTID:", 7) == 0)
        {
            nextCustomerId = atoi(line + 7);
            if (nextCustomerId < 1)
                nextCustomerId = 1;
        }
        else if (strncmp(line, "ID|", 3) == 0)
        {
            Customer c = {0};
            char tmp[1024];
            strncpy(tmp, line, sizeof(tmp) - 1);
            char *tok = strtok(tmp, "|");
            tok = strtok(NULL, "|");
            if (tok)
                c.id = atoi(tok);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                strncpy(c.name, tok, NAME_LEN - 1);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                strncpy(c.licenseNo, tok, LICENSE_LEN - 1);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                strncpy(c.contact, tok, CONTACT_LEN - 1);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                strncpy(c.email, tok, EMAIL_LEN - 1);
            if (c.id >= nextCustomerId)
                nextCustomerId = c.id + 1;
            if (customerCount < MAX_CUSTOMERS)
                customers[customerCount++] = c;
        }
    }
    fclose(fp);
}

void saveRentals()
{
    FILE *fp = fopen(DB_RENTALS_FILE, "w");
    if (!fp)
        return;
    fprintf(fp, "# Rentals file\n");
    fprintf(fp, "RENTALS:%d\nNEXTID:%d\n", rentalCount, nextRentalId);
    for (int i = 0; i < rentalCount; i++)
    {
        RentalRecord *r = &rentals[i];
        fprintf(fp,
                "ID|%d|BARCODE|%s|PLATE|%s|MODEL|%s|CUST|%s|LICENSE|%s|QTY|%d|DAYS|%d|AMT|%.2f|RENTTIME|%ld|RETURNED|%d|RETURNTIME|%ld\n",
                r->id, r->barcode, r->plateNumber, r->model, r->customerName, r->licenseNo,
                r->quantity, r->days, r->totalAmount, (long)r->rentTime, r->returned, (long)r->returnTime);
    }
    fclose(fp);
}

void loadRentals()
{
    rentalCount = 0;
    nextRentalId = 1;
    FILE *fp = fopen(DB_RENTALS_FILE, "r");
    if (!fp)
        return;
    char line[2048];
    while (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, "NEXTID:", 7) == 0)
        {
            nextRentalId = atoi(line + 7);
            if (nextRentalId < 1)
                nextRentalId = 1;
        }
        else if (strncmp(line, "ID|", 3) == 0)
        {
            RentalRecord r = {0};
            char tmp[2048];
            strncpy(tmp, line, sizeof(tmp) - 1);
            char *tok = strtok(tmp, "|");
            tok = strtok(NULL, "|");
            if (tok)
                r.id = atoi(tok);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                strncpy(r.barcode, tok, BARCODE_LEN - 1);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                strncpy(r.plateNumber, tok, PLATE_LEN - 1);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                strncpy(r.model, tok, MODEL_LEN - 1);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                strncpy(r.customerName, tok, NAME_LEN - 1);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                strncpy(r.licenseNo, tok, LICENSE_LEN - 1);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                r.quantity = atoi(tok);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                r.days = atoi(tok);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                r.totalAmount = atof(tok);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                r.rentTime = (time_t)atol(tok);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                r.returned = atoi(tok);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                r.returnTime = (time_t)atol(tok);
            if (r.id >= nextRentalId)
                nextRentalId = r.id + 1;
            if (rentalCount < MAX_RENTALS)
                rentals[rentalCount++] = r;
        }
    }
    fclose(fp);
}

void saveUsers()
{
    FILE *fp = fopen(DB_USERS_FILE, "w");
    if (!fp)
        return;
    fprintf(fp, "# Users file\n");
    fprintf(fp, "USERS:%d\nNEXTID:%d\n", userCount, nextUserId);
    for (int i = 0; i < userCount; i++)
    {
        User *u = &users[i];
        fprintf(fp, "ID|%d|USERNAME|%s|PASSWORD|%s|ADMIN|%d\n",
                u->id, u->username, u->password, u->isAdmin);
    }
    fclose(fp);
}

void loadUsers()
{
    userCount = 0;
    nextUserId = 1;
    FILE *fp = fopen(DB_USERS_FILE, "r");
    if (!fp)
        return;
    char line[1024];
    while (fgets(line, sizeof(line), fp))
    {
        if (strncmp(line, "NEXTID:", 7) == 0)
        {
            nextUserId = atoi(line + 7);
            if (nextUserId < 1)
                nextUserId = 1;
        }
        else if (strncmp(line, "ID|", 3) == 0)
        {
            User u = {0};
            char tmp[1024];
            strncpy(tmp, line, sizeof(tmp) - 1);
            char *tok = strtok(tmp, "|");
            tok = strtok(NULL, "|");
            if (tok)
                u.id = atoi(tok);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                strncpy(u.username, tok, USERNAME_LEN - 1);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                strncpy(u.password, tok, PASSWORD_LEN - 1);
            tok = strtok(NULL, "|");
            tok = strtok(NULL, "|");
            if (tok)
                u.isAdmin = atoi(tok);
            if (u.id >= nextUserId)
                nextUserId = u.id + 1;
            if (userCount < MAX_USERS)
                users[userCount++] = u;
        }
    }
    fclose(fp);
}

/* DEFAULT DATA */

void addDefaultCars()
{
    struct
    {
        const char *plate;
        const char *model;
        double rate;
        int avail;
    } d[] = {
        {"CAR-001", "Toyota Vios", 1800, 60}, {"CAR-002", "Honda Civic", 2500, 60}, {"CAR-003", "Mitsubishi Mirage", 1600, 60}, {"CAR-004", "Ford Ranger", 3200, 60}, {"CAR-005", "Hyundai Tucson", 2800, 60}, {"CAR-006", "Kia Picanto", 1500, 60}, {"CAR-007", "Nissan Almera", 1700, 60}, {"CAR-008", "Suzuki Ertiga", 2000, 60}, {"CAR-009", "Toyota Fortuner", 3500, 60}, {"CAR-010", "Honda BR-V", 2700, 60}, {"CAR-011", "Mazda 3 Sedan", 2600, 60}, {"CAR-012", "Chevrolet Trailblazer", 3300, 60}, {"CAR-013", "Ford Everest", 3400, 60}, {"CAR-014", "Mitsubishi Montero", 3600, 60}, {"CAR-015", "Toyota Innova", 2800, 60}, {"CAR-016", "Suzuki Swift", 1800, 60}, {"CAR-017", "Nissan Terra", 3400, 60}, {"CAR-018", "Hyundai Accent", 1600, 60}, {"CAR-019", "Isuzu mu-X", 3700, 60}, {"CAR-020", "Toyota Hilux", 3200, 60}, {"CAR-021", "BMW 320i", 5500, 60}, {"CAR-022", "BMW X5", 7200, 60}, {"CAR-023", "Mercedes C200", 6000, 60}, {"CAR-024", "Mercedes GLC", 7200, 60}, {"CAR-025", "Audi A4", 5800, 60}, {"CAR-026", "Audi Q7", 7500, 60}, {"CAR-027", "Volkswagen Golf", 4000, 60}, {"CAR-028", "Volkswagen Touareg", 6900, 60}, {"CAR-029", "Porsche Macan", 8900, 60}, {"CAR-030", "Porsche Cayenne", 11000, 60}};

    int n = sizeof(d) / sizeof(d[0]);
    for (int i = 0; i < n && carCount < MAX_CARS; i++)
    {
        Car c = {0};
        c.id = nextCarId++;
        snprintf(c.barcode, BARCODE_LEN, "%d", c.id);
        strncpy(c.plateNumber, d[i].plate, PLATE_LEN - 1);
        strncpy(c.model, d[i].model, MODEL_LEN - 1);
        c.pricePerDay = d[i].rate;
        c.available = d[i].avail;
        c.totalRentedCount = 0;
        inventory[carCount++] = c;
    }
}

void addDefaultUsers()
{
    struct
    {
        const char *username;
        const char *password;
        int isAdmin;
    } d[] = {
        {"admin", "admin123", 1},
        {"cashier", "cashier123", 0}};

    int n = sizeof(d) / sizeof(d[0]);
    for (int i = 0; i < n && userCount < MAX_USERS; i++)
    {
        User u = {0};
        u.id = nextUserId++;
        strncpy(u.username, d[i].username, USERNAME_LEN - 1);
        strncpy(u.password, d[i].password, PASSWORD_LEN - 1);
        u.isAdmin = d[i].isAdmin;
        users[userCount++] = u;
    }
}

void initializeDefaultDataIfEmpty()
{
    if (!fileExists(DB_USERS_FILE) || userCount == 0)
    {
        if (!fileExists(DB_USERS_FILE))
        {
            FILE *fu = fopen(DB_USERS_FILE, "w");
            if (fu)
            {
                fprintf(fu, "# Users file\nUSERS:0\nNEXTID:1\n");
                fclose(fu);
            }
        }
        if (userCount == 0)
        {
            addDefaultUsers();
            saveUsers();
        }
    }
    if (!fileExists(DB_INVENTORY_FILE) || carCount == 0)
    {
        carCount = 0;
        nextCarId = 1001;
        addDefaultCars();
        saveInventory();
    }
    if (!fileExists(DB_CUSTOMERS_FILE) || customerCount == 0)
    {
        customerCount = 0;
        nextCustomerId = 1;
        saveCustomers();
    }
    if (!fileExists(DB_RENTALS_FILE) || rentalCount == 0)
    {
        rentalCount = 0;
        nextRentalId = 1;
        saveRentals();
    }
    ensureReceiptsDir();
}

void initializeSystem()
{
    enableTerminalColors();
    curl_global_init(CURL_GLOBAL_DEFAULT);
    loadUsers();
    loadInventory();
    loadCustomers();
    loadRentals();
    initializeDefaultDataIfEmpty();
}

/* SEARCH FUNCTIONS */

int findCarByBarcode(const char *barcode)
{
    for (int i = 0; i < carCount; i++)
        if (strcmp(inventory[i].barcode, barcode) == 0)
            return i;
    return -1;
}

int findCarByPlate(const char *plate)
{
    for (int i = 0; i < carCount; i++)
        if (STRICMP(inventory[i].plateNumber, plate) == 0)
            return i;
    return -1;
}

int findCustomerByLicense(const char *license)
{
    for (int i = 0; i < customerCount; i++)
        if (STRICMP(customers[i].licenseNo, license) == 0)
            return i;
    return -1;
}

int findCustomerByName(const char *name)
{
    for (int i = 0; i < customerCount; i++)
        if (STRICMP(customers[i].name, name) == 0)
            return i;
    return -1;
}

int findUserByUsername(const char *username)
{
    for (int i = 0; i < userCount; i++)
        if (strcmp(users[i].username, username) == 0)
            return i;
    return -1;
}

/* EMAIL FUNCTIONS */

size_t curl_payload_read(void *ptr, size_t size, size_t nmemb, void *userp)
{
    const char **payload_text = (const char **)userp;
    const char *data = *payload_text;
    if (!data)
        return 0;
    size_t room = size * nmemb;
    size_t len = strlen(data);
    if (len > room)
        len = room;
    memcpy(ptr, data, len);
    *payload_text += len;
    return len;
}

int sendEmailLibCurl(const char *toAddress, const char *subject, const char *body)
{
    if (!toAddress || strlen(toAddress) < 5)
        return 0;

    CURL *curl = curl_easy_init();
    if (!curl)
        return 0;

    CURLcode res;
    char payload[9000];
    char fromHeader[256];

    snprintf(fromHeader, sizeof(fromHeader),
             "%s <%s>", EMAIL_SENDER_NAME, EMAIL_SENDER_ADDRESS);

    snprintf(payload, sizeof(payload),
             "From: %s\r\n"
             "To: %s\r\n"
             "Subject: %s\r\n"
             "MIME-Version: 1.0\r\n"
             "Content-Type: text/plain; charset=utf-8\r\n"
             "\r\n"
             "%s\r\n",
             fromHeader, toAddress, subject, body);

    const char *payload_ptr = payload;

    curl_easy_setopt(curl, CURLOPT_URL, "smtps://smtp.gmail.com:465");
    curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);

    curl_easy_setopt(curl, CURLOPT_USERNAME, EMAIL_SENDER_ADDRESS);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, EMAIL_APP_PASSWORD);

    curl_easy_setopt(curl, CURLOPT_LOGIN_OPTIONS, "AUTH=PLAIN");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    char mailfrom[256];
    snprintf(mailfrom, sizeof(mailfrom), "<%s>", EMAIL_SENDER_ADDRESS);
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, mailfrom);

    struct curl_slist *recipients = NULL;
    char rcpt[256];
    snprintf(rcpt, sizeof(rcpt), "<%s>", toAddress);
    recipients = curl_slist_append(recipients, rcpt);
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

    curl_easy_setopt(curl, CURLOPT_READFUNCTION, curl_payload_read);
    curl_easy_setopt(curl, CURLOPT_READDATA, &payload_ptr);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)strlen(payload));

    res = curl_easy_perform(curl);

    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        setColor(CLR_RED);
        printf("Email failed: %s\n", curl_easy_strerror(res));
        resetColor();
        return 0;
    }

    setColor(CLR_GREEN);
    printf("Email sent successfully to %s\n", toAddress);
    resetColor();
    return 1;
}

/* BARCODE FUNCTIONS */

void printAsciiBarcode(const char *code, FILE *out)
{
    const char *patterns[10] = {
        "|| ||",    // 0
        "| |||",    // 1
        "||| |",    // 2
        "|| |||",   // 3
        "| || ||",  // 4
        "|||| ",    // 5
        " ||||",    // 6
        "||| ||",   // 7
        "|| || ||", // 8
        "|||||"     // 9
    };

    fprintf(out, "\nBARCODE VISUAL:\n");
    for (int i = 0; code[i]; i++)
    {
        if (isdigit((unsigned char)code[i]))
        {
            int d = code[i] - '0';
            fprintf(out, "%s ", patterns[d]);
        }
        else
        {
            fprintf(out, "%c ", code[i]);
        }
    }
    fprintf(out, "\n%s\n", code);
}

void generateBarcodePNG(const char *code, const char *outputFile)
{
    if (!code || !outputFile)
        return;

    char command[512];
    snprintf(command, sizeof(command), "zint -o \"%s\" -d \"%s\" --barcode=20 --scale=3 --quiet", outputFile, code);

    int rc = system(command);
    if (rc != 0)
    {
        setColor(CLR_YELLOW);
        printf("Note: Zint barcode generation returned %d. Make sure zint is in PATH.\n", rc);
        resetColor();
    }
}

void autoOpenFile(const char *filename)
{
#ifdef _WIN32
    if (fileExists(filename))
        ShellExecuteA(NULL, "open", filename, NULL, NULL, SW_SHOWNORMAL);
#endif
}

/* RECEIPT FUNCTIONS */

void autoPrintReceipt(const char *filename)
{
#ifdef _WIN32
    char command[512];
    snprintf(command, sizeof(command), "notepad /p \"%s\"", filename);
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (CreateProcessA(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, 3000);
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
#endif
}

void printReceiptFromRental(const RentalRecord *r)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    ensureReceiptsDir();

    char filename[FILENAME_LEN];
    snprintf(filename, sizeof(filename), "%s/receipt_%d.txt", RECEIPTS_DIR, r->id);
    FILE *fp = fopen(filename, "w");
    if (!fp)
        return;

    fprintf(fp, "========================================\n");
    fprintf(fp, "  ███████╗  ██████╗ ██╗   ██╗██████╗     \n");
    fprintf(fp, "  ██╔════╝ ██╔═══██╗██║   ██║██╔══██╗    \n");
    fprintf(fp, "  █████╗   ██║   ██║██║   ██║██████╔╝    \n");
    fprintf(fp, "  ██╔══╝   ██║   ██║██║   ██║██╔══██╗    \n");
    fprintf(fp, "  ██║      ╚██████╔╝╚██████╔╝██║  ██║    \n");
    fprintf(fp, "  ╚═╝       ╚═════╝  ╚═════╝ ╚═╝  ╚═╝    \n");
    fprintf(fp, "      FOURSIGHT CAR RENTALS              \n");
    fprintf(fp, "========================================\n");
    fprintf(fp, "Date: %02d/%02d/%04d  Time: %02d:%02d:%02d\n",
            t->tm_mday, t->tm_mon + 1, t->tm_year + 1900, t->tm_hour, t->tm_min, t->tm_sec);
    fprintf(fp, "Rental ID: %d\nCustomer: %s\nLicense: %s\n", r->id, r->customerName, r->licenseNo);
    fprintf(fp, "Car: %s (%s)\nBarcode: %s\n", r->model, r->plateNumber, r->barcode);
    printAsciiBarcode(r->barcode, fp);
    fprintf(fp, "Qty: %d  Days: %d\n", r->quantity, r->days);
    fprintf(fp, "Total: PHP %.2f\n", r->totalAmount);
    fprintf(fp, "Returned: %s\n", r->returned ? "Yes" : "No");
    fprintf(fp, "========================================\n");
    fclose(fp);

    setColor(CLR_YELLOW);
    printf("Receipt saved to %s\n", filename);
    resetColor();


    autoPrintReceipt(filename);
}

int saveSimpleReceipt(int rentalId, const char *customerName, const char *license,
                      const char *model, const char *plate, const char *barcode,
                      int qty, int days, double total)
{
    ensureReceiptsDir();

    char txtfile[FILENAME_LEN];
    snprintf(txtfile, sizeof(txtfile), "%s/receipt_simple_%d.txt", RECEIPTS_DIR, rentalId);

    FILE *fp = fopen(txtfile, "w");
    if (!fp)
        return 0;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    fprintf(fp, "========================================\n");
    fprintf(fp, "  ███████╗  ██████╗ ██╗   ██╗██████╗     \n");
    fprintf(fp, "  ██╔════╝ ██╔═══██╗██║   ██║██╔══██╗    \n");
    fprintf(fp, "  █████╗   ██║   ██║██║   ██║██████╔╝    \n");
    fprintf(fp, "  ██╔══╝   ██║   ██║██║   ██║██╔══██╗    \n");
    fprintf(fp, "  ██║      ╚██████╔╝╚██████╔╝██║  ██║    \n");
    fprintf(fp, "  ╚═╝       ╚═════╝  ╚═════╝ ╚═╝  ╚═╝    \n");
    fprintf(fp, "      FOURSIGHT CAR RENTALS              \n");
    fprintf(fp, "========================================\n");
    fprintf(fp, "Date: %02d/%02d/%04d %02d:%02d:%02d\n",
            t->tm_mday, t->tm_mon + 1, t->tm_year + 1900,
            t->tm_hour, t->tm_min, t->tm_sec);
    fprintf(fp, "Rental ID: %d\n", rentalId);
    fprintf(fp, "Customer: %s\nLicense: %s\n\n", customerName, license);
    fprintf(fp, "Car: %s (%s)\n", model, plate);
    fprintf(fp, "Barcode: %s\n\n", barcode);
    fprintf(fp, "Qty: %d  Days: %d\n", qty, days);
    fprintf(fp, "Total: PHP %.2f\n", total);
    fprintf(fp, "========================================\n");

    printAsciiBarcode(barcode, fp);

    fclose(fp);

    autoPrintReceipt(txtfile);

    setColor(CLR_YELLOW);
    printf("Saved receipt: %s\n", txtfile);
    resetColor();

    return 1;
}

/* NEW FUNCTION: Save bulk receipt with multiple items */
void saveBulkReceipt(int transactionId,
                     const char *custName,
                     const char *custLicense,
                     const char *custContact,
                     const char *custEmail,
                     const char *cashierName,
                     RentalItem items[],
                     int itemCount,
                     double total,
                     double cash,
                     double change,
                     int firstRentalId,
                     int lastRentalId)
{
    ensureReceiptsDir();
    
    char filename[FILENAME_LEN];
    snprintf(filename, sizeof(filename), "%s/receipt_bulk_%d.txt", RECEIPTS_DIR, transactionId);

    FILE *fp = fopen(filename, "w");
    if (!fp)
        return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    time_t returnTime = now + (24 * 60 * 60); // 24 hours from now

    /* Print header similar to your first picture */
    fprintf(fp, "FOURSIGHT CAR RENTALS\n\n");
    fprintf(fp, "Date: %02d/%02d/%04d %02d:%02d:%02d\n",
            t->tm_mday, t->tm_mon + 1, t->tm_year + 1900,
            t->tm_hour, t->tm_min, t->tm_sec);
    fprintf(fp, "Cashier: %s\n", cashierName);
    fprintf(fp, "--- Customer: %s\n", custName);
    fprintf(fp, "License: %s\n", custLicense);
    fprintf(fp, "Contact: %s\n", custContact);
    fprintf(fp, "Email: %s\n", custEmail);
    fprintf(fp, "---\n");
    fprintf(fp, "=== RENTAL ITEMS ===\n");
    fprintf(fp, "Car Model    Plate    Qty    Days   Subtotal\n");
    fprintf(fp, "---\n");
    
    /* Print each rental item */
    for (int i = 0; i < itemCount; i++)
    {
        Car *car = &inventory[items[i].carIndex];
        fprintf(fp, "%-20s %-10s %-6d %-6d PHP %-8.2f\n",
                car->model, car->plateNumber,
                items[i].quantity, items[i].days,
                items[i].subtotal);
    }
    
    fprintf(fp, "\n");
    fprintf(fp, "TOTAL AMOUNT:    PHP    %.2f\n", total);
    fprintf(fp, "CASH PAID:    PHP    %.2f\n", cash);
    fprintf(fp, "CHANGE:    PHP    %.2f\n", change);
    fprintf(fp, "===========\n\n");
    
    fprintf(fp, "Rental IDs: %d to %d\n", firstRentalId, lastRentalId);
    fprintf(fp, "Total Items Rented: %d\n\n", itemCount);
    
    fprintf(fp, "Thank you for choosing FOURSIGHT Car Rentals!\n");
    fprintf(fp, "Please return all cars by: %s", ctime(&returnTime));
    fprintf(fp, "============================\n\n");
    
    /* Generate and list barcodes for each rental item */
    fprintf(fp, "=== BARCODES FOR EACH RENTAL ===\n");
    for (int i = 0; i < itemCount; i++)
    {
        Car *car = &inventory[items[i].carIndex];
        fprintf(fp, "Rental ID %d: %s - %s\n", 
                firstRentalId + i, car->model, car->barcode);
        printAsciiBarcode(car->barcode, fp);
        fprintf(fp, "\n");
    }
    
    fclose(fp);
    
    /* Generate barcode PNGs for each rental item */
    for (int i = 0; i < itemCount; i++)
    {
        Car *car = &inventory[items[i].carIndex];
    }
    
    autoPrintReceipt(filename);
    
    setColor(CLR_GREEN);
    printf("Bulk receipt saved: %s\n", filename);
    printf("Generated %d individual barcode PNGs\n", itemCount);
    resetColor();
}

void reprintReceiptMenu()
{
    if (rentalCount == 0)
    {
        printf("No rentals found.\n");
        pressEnterToContinue();
        return;
    }

    printf("\n==== RENTAL LIST ====\n");
    printf("%-5s %-20s %-12s %-10s\n", "No.", "Customer", "License", "Returned");
    printf("----------------------------------------\n");

    for (int i = 0; i < rentalCount; i++)
    {
        printf("%-5d %-20s %-12s %-10s\n",
               i + 1,
               rentals[i].customerName,
               rentals[i].licenseNo,
               rentals[i].returned ? "YES" : "NO");
    }

    printf("\nSelect rental number (0 to cancel): ");
    int choice;
    if (scanf("%d", &choice) != 1)
    {
        flushInput();
        return;
    }
    flushInput();

    if (choice <= 0 || choice > rentalCount)
        return;

    printReceiptFromRental(&rentals[choice - 1]);
    pressEnterToContinue();
}

/* ============= LOGIN FUNCTION ============= */

int login(char *loggedUser, int *isAdmin)
{
    char username[USERNAME_LEN], password[PASSWORD_LEN];
    int roleChoice = 0;
    clearScreen();

    roleChoice = showLoginHeader();
    if (roleChoice == 0)
        return 0;

    printBanner();
    printf("\nType 'exit' as username to quit.\n\n");
    printf("Username: ");
    if (scanf("%39s", username) != 1)
        return 0;
    if (strcmp(username, "exit") == 0)
        return -1;

    printf("Password: ");

#ifdef _WIN32
    int i = 0;
    char ch;
    while (i < PASSWORD_LEN - 1)
    {
        ch = _getch();
        if (ch == '\r' || ch == '\n')
            break;
        if (ch == '\b')
        {
            if (i > 0)
            {
                i--;
                printf("\b \b");
            }
        }
        else if (ch >= 32 && ch <= 126)
        {
            password[i++] = ch;
            putchar('*');
        }
    }
    password[i] = '\0';
    printf("\n");
#else
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    int i = 0;
    int ch;
    while (i < PASSWORD_LEN - 1 && (ch = getchar()) != '\n' && ch != EOF)
    {
        if (ch == 127)
        {
            if (i > 0)
            {
                i--;
                printf("\b \b");
            }
        }
        else
        {
            password[i++] = ch;
            putchar('*');
        }
    }
    password[i] = '\0';
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    printf("\n");
#endif

    flushInput();
    int idx = findUserByUsername(username);
    if (idx == -1)
        return 0;

    if (roleChoice == 1 && users[idx].isAdmin == 0)
    {
        setColor(CLR_RED);
        printf("Selected Admin but account is not admin.\n");
        resetColor();
        return 0;
    }
    if (roleChoice == 2 && users[idx].isAdmin == 1)
    {
        setColor(CLR_RED);
        printf("Selected Cashier but account is admin.\n");
        resetColor();
        return 0;
    }

    if (strcmp(users[idx].password, password) == 0)
    {
        strncpy(loggedUser, username, USERNAME_LEN - 1);
        *isAdmin = users[idx].isAdmin;
        return 1;
    }
    return 0;
}

/* INVENTORY FUNCTIONS */

void showInventoryListDetailed()
{
    clearScreen();
    setColor(CLR_CYAN);
    printf("\n========================================\n");
    printf("            CAR INVENTORY\n");
    printf("========================================\n");
    resetColor();

    if (carCount == 0)
    {
        setColor(CLR_YELLOW);
        printf("No cars in inventory.\n");
        resetColor();
        return;
    }

    printf("%-6s %-10s %-28s %-12s %-10s\n", "ID", "Plate", "Model", "Rate/Day", "Available");
    printf("-------------------------------------------------------------------------------\n");

    for (int i = 0; i < carCount; i++)
    {
        printf("%-6d %-10s %-28s PHP %-8.2f %8d\n",
               inventory[i].id,
               inventory[i].plateNumber,
               inventory[i].model,
               inventory[i].pricePerDay,
               inventory[i].available);
        printAsciiBarcode(inventory[i].barcode, stdout);
        printf("-------------------------------------------------------------------------------\n");
    }
}

void viewInventoryInteractive()
{
    int ch;
    do
    {
        showInventoryListDetailed();
        printf("\nInventory Menu:\n1. Add Car\n2. Edit Car\n3. Delete Car\n4. Search Car\n5. Back\nChoice: ");

        if (scanf("%d", &ch) != 1)
        {
            flushInput();
            printf("Invalid input.\n");
            pressEnterToContinue();
            continue;
        }
        flushInput();

        if (ch == 1)
        {
            if (carCount >= MAX_CARS)
            {
                setColor(CLR_RED);
                printf("Inventory is full.\n");
                resetColor();
                pressEnterToContinue();
                continue;
            }

            Car c = {0};
            c.id = nextCarId++;
            safeInput(c.plateNumber, PLATE_LEN, "Enter plate number: ");
            safeInput(c.model, MODEL_LEN, "Enter model: ");

            printf("Enter price per day: ");
            if (scanf("%lf", &c.pricePerDay) != 1)
            {
                flushInput();
                setColor(CLR_RED);
                printf("Invalid price.\n");
                resetColor();
                pressEnterToContinue();
                continue;
            }

            printf("Enter available units: ");
            if (scanf("%d", &c.available) != 1)
            {
                flushInput();
                setColor(CLR_RED);
                printf("Invalid number.\n");
                resetColor();
                pressEnterToContinue();
                continue;
            }
            flushInput();

            c.totalRentedCount = 0;
            snprintf(c.barcode, BARCODE_LEN, "%d", c.id);
            inventory[carCount++] = c;
            saveInventory();
            ensureReceiptsDir();
            char pngfile[FILENAME_LEN];
            snprintf(pngfile, sizeof(pngfile),
            "%s/car_barcode_%s.png", RECEIPTS_DIR, c.barcode);
            generateBarcodePNG(c.barcode, pngfile);

            setColor(CLR_GREEN);
            printf("Car added successfully.\n");
            resetColor();

            printf("\nAdded Car:\n");
            printf("ID: %d Plate: %s Model: %s Price: PHP %.2f Avail: %d\n",
                   c.id, c.plateNumber, c.model, c.pricePerDay, c.available);
            printAsciiBarcode(c.barcode, stdout);
            pressEnterToContinue();
        }
        else if (ch == 2)
        {
            char key[BARCODE_LEN];
            safeInput(key, BARCODE_LEN, "Enter plate or barcode to edit: ");

            int idx = findCarByBarcode(key);
            if (idx == -1)
                idx = findCarByPlate(key);

            if (idx == -1)
            {
                setColor(CLR_RED);
                printf("Car not found.\n");
                resetColor();
                pressEnterToContinue();
                continue;
            }

            Car *p = &inventory[idx];
            printf("Editing %s (%s)\n", p->model, p->plateNumber);

            char tmp[128];
            safeInput(tmp, PLATE_LEN, "New plate (leave empty to keep): ");
            if (tmp[0])
                strncpy(p->plateNumber, tmp, PLATE_LEN - 1);

            safeInput(tmp, MODEL_LEN, "New model (leave empty to keep): ");
            if (tmp[0])
                strncpy(p->model, tmp, MODEL_LEN - 1);

            printf("New price per day (0 to keep): ");
            double pr;
            if (scanf("%lf", &pr) == 1)
            {
                if (pr > 0)
                    p->pricePerDay = pr;
            }
            flushInput();

            printf("New available units (negative to keep): ");
            int av;
            if (scanf("%d", &av) == 1)
            {
                if (av >= 0)
                    p->available = av;
            }
            flushInput();

            saveInventory();
            setColor(CLR_GREEN);
            printf("Car updated.\n");
            resetColor();

            printf("\nUpdated Car:\n");
            printf("ID: %d Plate: %s Model: %s Price: PHP %.2f Avail: %d\n",
                   p->id, p->plateNumber, p->model, p->pricePerDay, p->available);
            printAsciiBarcode(p->barcode, stdout);
            pressEnterToContinue();
        }
        else if (ch == 3)
        {
            char key[BARCODE_LEN];
            safeInput(key, BARCODE_LEN, "Enter plate or barcode to delete: ");

            int idx = findCarByBarcode(key);
            if (idx == -1)
                idx = findCarByPlate(key);

            if (idx == -1)
            {
                setColor(CLR_RED);
                printf("Car not found.\n");
                resetColor();
                pressEnterToContinue();
                continue;
            }

            printf("Are you sure you want to delete %s (%s)? (y/n): ",
                   inventory[idx].model, inventory[idx].plateNumber);
            char c = getchar();
            flushInput();

            if (c == 'y' || c == 'Y')
            {
                for (int i = idx; i < carCount - 1; i++)
                    inventory[i] = inventory[i + 1];
                carCount--;
                saveInventory();

                setColor(CLR_GREEN);
                printf("Car deleted.\n");
                resetColor();
            }
            else
            {
                printf("Delete cancelled.\n");
            }
            pressEnterToContinue();
        }
        else if (ch == 4)
        {
            char key[BARCODE_LEN];
            safeInput(key, BARCODE_LEN, "Enter plate or barcode to search: ");

            int idx = findCarByBarcode(key);
            if (idx == -1)
                idx = findCarByPlate(key);

            if (idx == -1)
            {
                setColor(CLR_YELLOW);
                printf("No matching car found.\n");
                resetColor();
                pressEnterToContinue();
                continue;
            }

            Car *p = &inventory[idx];
            setColor(CLR_CYAN);
            printf("\nFound Car:\n");
            resetColor();

            printf("ID: %d\nBarcode: %s\nPlate: %s\nModel: %s\nPrice/Day: PHP %.2f\nAvailable: %d\nTotal Rented: %d\n",
                   p->id, p->barcode, p->plateNumber, p->model, p->pricePerDay, p->available, p->totalRentedCount);
            printAsciiBarcode(p->barcode, stdout);
            pressEnterToContinue();
        }
        else if (ch == 5)
            break;
        else
        {
            printf("Invalid choice.\n");
            pressEnterToContinue();
        }
    } while (1);
}

void showQuickInventory()
{
    clearScreen();
    printf("\n=== Quick Inventory ===\n");
    printf("%-6s %-10s %-25s %-10s\n", "ID", "Plate", "Model", "Available");
    printf("---------------------------------------------------\n");

    for (int i = 0; i < carCount; i++)
    {
        printf("%-6d %-10s %-25s %8d\n",
               inventory[i].id,
               inventory[i].plateNumber,
               inventory[i].model,
               inventory[i].available);
    }
    printf("---------------------------------------------------\n");
}

/* CUSTOMER FUNCTIONS */

void listCustomers()
{
    clearScreen();
    setColor(CLR_CYAN);
    printf("\n========================================\n");
    printf("           CUSTOMER DATABASE\n");
    printf("========================================\n");
    resetColor();

    if (customerCount == 0)
    {
        setColor(CLR_YELLOW);
        printf("No customers found.\n");
        resetColor();
        return;
    }

    printf("%-6s %-28s %-18s %-18s %-30s\n",
           "ID", "Name", "License No.", "Contact", "Email");
    printf("-------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < customerCount; i++)
    {
        printf("%-6d %-28s %-18s %-18s %-30s\n",
               customers[i].id,
               customers[i].name,
               customers[i].licenseNo,
               customers[i].contact,
               customers[i].email);
    }
    printf("-------------------------------------------------------------------------------------------\n");
}

void addCustomer()
{
    if (customerCount >= MAX_CUSTOMERS)
    {
        setColor(CLR_RED);
        printf("Customer database full!\n");
        resetColor();
        pressEnterToContinue();
        return;
    }

    Customer c = {0};
    c.id = nextCustomerId++;

    safeInput(c.name, NAME_LEN, "Customer Name: ");
    safeInput(c.licenseNo, LICENSE_LEN, "License No.: ");
    safeInput(c.contact, CONTACT_LEN, "Contact Number: ");
    safeInput(c.email, EMAIL_LEN, "Email Address: ");

    customers[customerCount++] = c;
    saveCustomers();

    setColor(CLR_GREEN);
    printf("Customer added successfully!\n");
    resetColor();
    pressEnterToContinue();
}

void editCustomer()
{
    int idx = selectCustomerMenu();
    if (idx == -1)
        return;

    Customer *c = &customers[idx];
    printf("Editing customer: %s (%s)\n", c->name, c->licenseNo);

    char tmp[128];
    safeInput(tmp, NAME_LEN, "New name (leave blank to keep): ");
    if (tmp[0])
        strncpy(c->name, tmp, NAME_LEN - 1);

    safeInput(tmp, CONTACT_LEN, "New contact (leave blank to keep): ");
    if (tmp[0])
        strncpy(c->contact, tmp, CONTACT_LEN - 1);

    safeInput(tmp, EMAIL_LEN, "New email (leave blank to keep): ");
    if (tmp[0])
        strncpy(c->email, tmp, EMAIL_LEN - 1);

    saveCustomers();
    printf("Customer updated.\n");
    pressEnterToContinue();
}

void deleteCustomer()
{
    int idx = selectCustomerMenu();
    if (idx == -1)
        return;

    printf("Delete %s (%s)? (y/n): ", customers[idx].name, customers[idx].licenseNo);
    char c = getchar();
    flushInput();

    if (c == 'y' || c == 'Y')
    {
        for (int i = idx; i < customerCount - 1; i++)
            customers[i] = customers[i + 1];

        customerCount--;
        saveCustomers();
        printf("Customer deleted.\n");
    }

    pressEnterToContinue();
}

void searchCustomer()
{
    char key[LICENSE_LEN];
    safeInput(key, LICENSE_LEN, "Enter License No.: ");

    int idx = findCustomerByLicense(key);
    if (idx == -1)
    {
        setColor(CLR_YELLOW);
        printf("Customer not found.\n");
        resetColor();
        pressEnterToContinue();
        return;
    }

    Customer *c = &customers[idx];
    setColor(CLR_CYAN);
    printf("\nCustomer Found:\n");
    resetColor();
    printf("ID: %d\nName: %s\nLicense: %s\nContact: %s\nEmail: %s\n",
           c->id, c->name, c->licenseNo, c->contact, c->email);

    pressEnterToContinue();
}

void customerMenu()
{
    int ch;
    do
    {
        clearScreen();
        setColor(CLR_BLUE);
        printf("========================================\n");
        printf("            CUSTOMER MENU\n");
        printf("========================================\n");
        resetColor();

        printf("1. View Customers\n");
        printf("2. Add Customer\n");
        printf("3. Edit Customer\n");
        printf("4. Delete Customer\n");
        printf("5. Search Customer\n");
        printf("6. Back\n");
        printf("Choice: ");

        if (scanf("%d", &ch) != 1)
        {
            flushInput();
            continue;
        }
        flushInput();

        if (ch == 1)
        {
            listCustomers();
            pressEnterToContinue();
        }
        else if (ch == 2)
            addCustomer();
        else if (ch == 3)
            editCustomer();
        else if (ch == 4)
            deleteCustomer();
        else if (ch == 5)
            searchCustomer();
        else if (ch == 6)
            break;
        else
        {
            printf("Invalid choice.\n");
            pressEnterToContinue();
        }

    } while (1);
}

int selectCustomerMenu()
{
    listCustomers();

    if (customerCount == 0)
    {
        printf("No customers available.\n");
        return -1;
    }

    printf("Select customer (0 to cancel): ");
    int choice;
    if (scanf("%d", &choice) != 1)
    {
        flushInput();
        return -1;
    }
    flushInput();

    if (choice == 0)
        return -1;

    if (choice < 1 || choice > customerCount)
        return -1;

    return choice - 1;
}

/* SIMPLIFIED RENTAL SYSTEM WITH BULK SUPPORT */

void rentCarMenu(const char *cashierName)
{
    clearScreen();
    setColor(CLR_BLUE);
    printf("========================================\n");
    printf("      RENT A CAR - BULK RENTAL SUPPORT\n");
    printf("========================================\n");
    resetColor();

    printf("Cashier: %s\n\n", cashierName);

    /* Step 1: Select or create customer */
    int customerIndex = -1;
    printf("1. Select existing customer\n");
    printf("2. Create new customer\n");
    printf("Choice: ");

    int choice;
    if (scanf("%d", &choice) != 1)
    {
        flushInput();
        printf("Invalid choice.\n");
        pressEnterToContinue();
        return;
    }
    flushInput();

    char custName[NAME_LEN] = "";
    char custLicense[LICENSE_LEN] = "";
    char custContact[CONTACT_LEN] = "";
    char custEmail[EMAIL_LEN] = "";

    if (choice == 1)
    {
        customerIndex = selectCustomerMenu();
        if (customerIndex == -1)
        {
            printf("No customer selected.\n");
            pressEnterToContinue();
            return;
        }

        strncpy(custName, customers[customerIndex].name, NAME_LEN - 1);
        strncpy(custLicense, customers[customerIndex].licenseNo, LICENSE_LEN - 1);
        strncpy(custContact, customers[customerIndex].contact, CONTACT_LEN - 1);
        strncpy(custEmail, customers[customerIndex].email, EMAIL_LEN - 1);

        printf("Selected customer: %s\n", custName);
    }
    else if (choice == 2)
    {
        if (customerCount >= MAX_CUSTOMERS)
        {
            setColor(CLR_RED);
            printf("Customer database full!\n");
            resetColor();
            pressEnterToContinue();
            return;
        }

        Customer c = {0};
        c.id = nextCustomerId++;

        safeInput(c.name, NAME_LEN, "Customer Name: ");
        safeInput(c.licenseNo, LICENSE_LEN, "License No.: ");
        safeInput(c.contact, CONTACT_LEN, "Contact Number: ");
        safeInput(c.email, EMAIL_LEN, "Email Address: ");

        customers[customerCount++] = c;
        saveCustomers();
        customerIndex = customerCount - 1;

        strncpy(custName, c.name, NAME_LEN - 1);
        strncpy(custLicense, c.licenseNo, LICENSE_LEN - 1);
        strncpy(custContact, c.contact, CONTACT_LEN - 1);
        strncpy(custEmail, c.email, EMAIL_LEN - 1);

        setColor(CLR_GREEN);
        printf("New customer created: %s\n", custName);
        resetColor();
    }
    else
    {
        printf("Invalid choice.\n");
        pressEnterToContinue();
        return;
    }

    /* Step 2: Bulk rental process */
    RentalItem rentalItems[MAX_CARS]; /* Array to store multiple rental items */
    int itemCount = 0;
    double totalAmount = 0.0;
    char addMore = 'y';

    do
    {
        /* Show available cars */
        showQuickInventory();

        /* Select car */
        char barcode[BARCODE_LEN];
        printf("\nEnter barcode of car to rent: ");
        safeInput(barcode, BARCODE_LEN, "");

        int carIndex = findCarByBarcode(barcode);
        if (carIndex == -1)
        {
            setColor(CLR_RED);
            printf("Car not found with barcode: %s\n", barcode);
            resetColor();
            pressEnterToContinue();
            continue;
        }

        Car *selectedCar = &inventory[carIndex];

        if (selectedCar->available <= 0)
        {
            setColor(CLR_RED);
            printf("Sorry, %s is not available for rent.\n", selectedCar->model);
            resetColor();
            pressEnterToContinue();
            continue;
        }

        /* Check if this car is already in the rental list */
        int existingItem = -1;
        for (int i = 0; i < itemCount; i++)
        {
            if (rentalItems[i].carIndex == carIndex)
            {
                existingItem = i;
                break;
            }
        }

        /* Get rental details */
        int quantity = 1;
        int days = 1;

        if (existingItem == -1)
        {
            printf("How many units of %s? (Available: %d): ",
                   selectedCar->model, selectedCar->available);
            if (scanf("%d", &quantity) != 1 || quantity <= 0)
            {
                flushInput();
                printf("Invalid quantity.\n");
                pressEnterToContinue();
                continue;
            }

            if (quantity > selectedCar->available)
            {
                setColor(CLR_RED);
                printf("Not enough units available. Only %d available.\n",
                       selectedCar->available);
                resetColor();
                pressEnterToContinue();
                continue;
            }
        }
        else
        {
            printf("This car is already in your rental list with %d units.\n",
                   rentalItems[existingItem].quantity);
            printf("How many additional units? (Available: %d): ",
                   selectedCar->available - rentalItems[existingItem].quantity);
            if (scanf("%d", &quantity) != 1 || quantity <= 0)
            {
                flushInput();
                printf("Invalid quantity.\n");
                pressEnterToContinue();
                continue;
            }

            if (quantity > (selectedCar->available - rentalItems[existingItem].quantity))
            {
                setColor(CLR_RED);
                printf("Not enough units available. Only %d additional units available.\n",
                       selectedCar->available - rentalItems[existingItem].quantity);
                resetColor();
                pressEnterToContinue();
                continue;
            }
        }

        printf("How many days to rent? (Minimum 1): ");
        if (scanf("%d", &days) != 1 || days <= 0)
        {
            flushInput();
            printf("Invalid number of days.\n");
            pressEnterToContinue();
            continue;
        }
        flushInput();

        /* Calculate subtotal */
        double subtotal = selectedCar->pricePerDay * quantity * days;

        if (existingItem == -1)
        {
            /* Add new item */
            if (itemCount >= MAX_CARS)
            {
                printf("Cannot add more items. Maximum rental items reached.\n");
                pressEnterToContinue();
                continue;
            }

            rentalItems[itemCount].carIndex = carIndex;
            rentalItems[itemCount].quantity = quantity;
            rentalItems[itemCount].days = days;
            rentalItems[itemCount].subtotal = subtotal;
            itemCount++;

            totalAmount += subtotal;

            setColor(CLR_GREEN);
            printf("Added %d units of %s for %d days (PHP %.2f)\n",
                   quantity, selectedCar->model, days, subtotal);
            resetColor();
        }
        else
        {
            /* Update existing item */
            rentalItems[existingItem].quantity += quantity;
            rentalItems[existingItem].days = days; /* Update days for all units */
            rentalItems[existingItem].subtotal = selectedCar->pricePerDay *
                                                 rentalItems[existingItem].quantity * days;

            /* Recalculate total */
            totalAmount = 0;
            for (int i = 0; i < itemCount; i++)
            {
                totalAmount += rentalItems[i].subtotal;
            }

            setColor(CLR_GREEN);
            printf("Updated %s to %d units for %d days (PHP %.2f)\n",
                   selectedCar->model, rentalItems[existingItem].quantity,
                   days, rentalItems[existingItem].subtotal);
            resetColor();
        }

        /* Show current rental summary */
        printf("\n=== CURRENT RENTAL SUMMARY ===\n");
        printf("%-25s %-10s %-6s %-12s\n", "Car Model", "Quantity", "Days", "Subtotal");
        printf("------------------------------------------------------------\n");

        for (int i = 0; i < itemCount; i++)
        {
            Car *car = &inventory[rentalItems[i].carIndex];
            printf("%-25s %-10d %-6d PHP %-8.2f\n",
                   car->model, rentalItems[i].quantity,
                   rentalItems[i].days, rentalItems[i].subtotal);
        }
        printf("------------------------------------------------------------\n");
        printf("TOTAL: PHP %.2f\n\n", totalAmount);

        /* Ask if user wants to add another car */
        if (itemCount < MAX_CARS)
        {
            printf("Add another car? (y/n): ");
            addMore = getchar();
            flushInput();
        }
        else
        {
            printf("Maximum items reached. Proceeding to payment.\n");
            addMore = 'n';
        }

    } while ((addMore == 'y' || addMore == 'Y') && itemCount < MAX_CARS);

    if (itemCount == 0)
    {
        printf("No cars selected. Rental cancelled.\n");
        pressEnterToContinue();
        return;
    }

    /* Step 3: Show final invoice */
    clearScreen();
    setColor(CLR_CYAN);
    printf("========================================\n");
    printf("               FINAL INVOICE\n");
    printf("========================================\n");
    resetColor();

    printf("Customer: %s\n", custName);
    printf("License: %s\n", custLicense);
    printf("Contact: %s\n", custContact);
    printf("\n=== RENTAL ITEMS ===\n");
    printf("%-25s %-12s %-10s %-6s %-12s\n",
           "Car Model", "Plate", "Quantity", "Days", "Subtotal");
    printf("----------------------------------------------------------------\n");

    for (int i = 0; i < itemCount; i++)
    {
        Car *car = &inventory[rentalItems[i].carIndex];
        printf("%-25s %-12s %-10d %-6d PHP %-8.2f\n",
               car->model, car->plateNumber,
               rentalItems[i].quantity, rentalItems[i].days,
               rentalItems[i].subtotal);
    }
    printf("----------------------------------------------------------------\n");
    printf("TOTAL AMOUNT: PHP %.2f\n\n", totalAmount);

    printf("Confirm rental? (y/n): ");
    char confirm = getchar();
    flushInput();

    if (confirm != 'y' && confirm != 'Y')
    {
        printf("Rental cancelled.\n");
        pressEnterToContinue();
        return;
    }

    /* Step 4: Process payment */
    double cash = 0.0;
    while (1)
    {
        printf("Enter cash amount: PHP ");
        if (scanf("%lf", &cash) != 1)
        {
            flushInput();
            printf("Invalid amount.\n");
            continue;
        }
        flushInput();

        if (cash < totalAmount)
        {
            printf("Insufficient cash. Need PHP %.2f more.\n", totalAmount - cash);
            continue;
        }
        break;
    }

    double change = cash - totalAmount;
    printf("Change: PHP %.2f\n", change);

    /* Step 5: Create rental records and update inventory */
    int firstRentalId = nextRentalId;
    
    for (int i = 0; i < itemCount; i++)
    {
        Car *car = &inventory[rentalItems[i].carIndex];

        /* Create rental record for each item */
        if (rentalCount >= MAX_RENTALS)
        {
            setColor(CLR_RED);
            printf("Rental database full! Could not save all rentals.\n");
            resetColor();
            break;
        }

        RentalRecord rental = {0};
        rental.id = nextRentalId++;
        strncpy(rental.barcode, car->barcode, BARCODE_LEN - 1);
        strncpy(rental.plateNumber, car->plateNumber, PLATE_LEN - 1);
        strncpy(rental.model, car->model, MODEL_LEN - 1);
        strncpy(rental.customerName, custName, NAME_LEN - 1);
        strncpy(rental.licenseNo, custLicense, LICENSE_LEN - 1);
        rental.quantity = rentalItems[i].quantity;
        rental.days = rentalItems[i].days;
        rental.totalAmount = rentalItems[i].subtotal;
        rental.rentTime = time(NULL);
        rental.returned = 0;
        rental.returnTime = 0;

        rentals[rentalCount++] = rental;

        /* Update inventory */
        car->available -= rentalItems[i].quantity;
        car->totalRentedCount += rentalItems[i].quantity;
    }

    saveRentals();
    saveInventory();

    /* Step 6: Generate receipts and barcodes */
    int lastRentalId = nextRentalId - 1;
    int transactionId = firstRentalId; /* Use first rental ID as transaction ID */

    /* Save bulk receipt (single receipt with all items) */
    saveBulkReceipt(transactionId,
                    custName,
                    custLicense,
                    custContact,
                    custEmail,
                    cashierName,
                    rentalItems,
                    itemCount,
                    totalAmount,
                    cash,
                    change,
                    firstRentalId,
                    lastRentalId);

    /* Also save individual receipts for each rental */
    for (int i = 0; i < itemCount; i++)
    {
        Car *car = &inventory[rentalItems[i].carIndex];
        saveSimpleReceipt(firstRentalId + i, custName, custLicense, car->model,
                          car->plateNumber, car->barcode,
                          rentalItems[i].quantity, rentalItems[i].days,
                          rentalItems[i].subtotal);
    }

    /* Step 7: Send email confirmation */
    if (strlen(custEmail) > 4)
    {
        char subject[256];
        snprintf(subject, sizeof(subject),
                 "Bulk Rental Confirmation - FOURSIGHT");

        char body[4096];
        char *bodyPtr = body;

        bodyPtr += snprintf(body, sizeof(body),
                            "Hello %s,\n\n"
                            "Thank you for choosing FOURSIGHT Car Rentals!\n\n"
                            "BULK RENTAL DETAILS:\n"
                            "------------------------------------------------\n"
                            "Total Items: %d\n"
                            "Total Amount: PHP %.2f\n"
                            "Rental Date: %s\n\n"
                            "Items Rented:\n"
                            "------------------------------------------------\n",
                            custName, itemCount, totalAmount,
                            ctime(&rentals[rentalCount - itemCount].rentTime));

        for (int i = 0; i < itemCount && (size_t)(bodyPtr - body) < (sizeof(body) - 200); i++)
        {
            Car *car = &inventory[rentalItems[i].carIndex];
            bodyPtr += snprintf(bodyPtr, sizeof(body) - (bodyPtr - body),
                                "%d. %s (%s)\n"
                                "   Quantity: %d, Days: %d, Amount: PHP %.2f\n\n",
                                i + 1, car->model, car->plateNumber,
                                rentalItems[i].quantity, rentalItems[i].days,
                                rentalItems[i].subtotal);
        }

        bodyPtr += snprintf(bodyPtr, sizeof(body) - (bodyPtr - body),
                            "------------------------------------------------\n"
                            "Please keep this email for your records.\n\n"
                            "Drive safely!\n");

        sendEmailLibCurl(custEmail, subject, body);
    }

    setColor(CLR_GREEN);
    printf("\nBulk rental completed successfully!\n");
    printf("Total Items: %d | Total Amount: PHP %.2f\n", itemCount, totalAmount);
    printf("Rental IDs: %d to %d\n",
           firstRentalId, lastRentalId);
    resetColor();
    pressEnterToContinue();
}

/* RETURN CAR FUNCTION */

void returnCar()
{
    clearScreen();
    setColor(CLR_CYAN);
    printf("========================================\n");
    printf("           RETURN A RENTAL CAR\n");
    printf("========================================\n");
    resetColor();

    printf("Enter Rental ID to return: ");
    int id;
    if (scanf("%d", &id) != 1)
    {
        flushInput();
        return;
    }
    flushInput();

    int idx = -1;
    for (int i = 0; i < rentalCount; i++)
        if (rentals[i].id == id)
            idx = i;

    if (idx == -1)
    {
        setColor(CLR_RED);
        printf("Rental not found.\n");
        resetColor();
        pressEnterToContinue();
        return;
    }

    RentalRecord *r = &rentals[idx];
    if (r->returned)
    {
        setColor(CLR_YELLOW);
        printf("Car already returned.\n");
        resetColor();
        pressEnterToContinue();
        return;
    }

    int carIdx = findCarByBarcode(r->barcode);
    if (carIdx != -1)
        inventory[carIdx].available += r->quantity;

    r->returned = 1;
    r->returnTime = time(NULL);

    saveRentals();
    saveInventory();

    /* Generate return receipt */
    char filename[FILENAME_LEN];
    snprintf(filename, sizeof(filename), "%s/return_%d.txt", RECEIPTS_DIR, r->id);

    FILE *fp = fopen(filename, "w");
    if (fp)
    {
        fprintf(fp, "========================================\n");
        fprintf(fp, "          FOURSIGHT CAR RENTALS\n");
        fprintf(fp, "           RETURN CONFIRMATION\n");
        fprintf(fp, "========================================\n");
        fprintf(fp, "Rental ID: %d\n", r->id);
        fprintf(fp, "Customer: %s\nLicense: %s\n", r->customerName, r->licenseNo);
        fprintf(fp, "Car: %s (%s)\n", r->model, r->plateNumber);
        fprintf(fp, "Returned Quantity: %d\n", r->quantity);
        fprintf(fp, "Return Time: %s", ctime(&r->returnTime));
        fprintf(fp, "========================================\n");
        fclose(fp);
    }

    autoPrintReceipt(filename);

    /* Send return confirmation email */
    int customerIdx = findCustomerByLicense(r->licenseNo);
    if (customerIdx != -1 && strlen(customers[customerIdx].email) > 4)
    {
        char subject[256];
        snprintf(subject, sizeof(subject), "Return Confirmation - FOURSIGHT (Rental #%d)", r->id);

        char body[2048];
        snprintf(body, sizeof(body),
                 "Hello %s,\n\n"
                 "This email confirms that your rented car has been returned successfully.\n\n"
                 "Car Returned:\n"
                 "%s (%s)\n"
                 "Quantity: %d\n"
                 "Return Time: %s\n\n"
                 "Thank you for choosing FOURSIGHT Car Rentals!\n",
                 r->customerName, r->model, r->plateNumber,
                 r->quantity, ctime(&r->returnTime));

        sendEmailLibCurl(customers[customerIdx].email, subject, body);
    }

    setColor(CLR_GREEN);
    printf("Car returned successfully!\n");
    resetColor();
    pressEnterToContinue();
}

/* PAYMENT/BILLING MENU */

void paymentBillingMenu()
{
    int ch;

    do
    {
        clearScreen();
        setColor(CLR_BLUE);
        printf("========================================\n");
        printf("          PAYMENT / BILLING MENU\n");
        printf("========================================\n");
        resetColor();

        printf("1. Show Receipt by Rental ID\n");
        printf("2. Reprint Receipt\n");
        printf("3. Back\n");
        printf("Choice: ");

        if (scanf("%d", &ch) != 1)
        {
            flushInput();
            continue;
        }
        flushInput();

        if (ch == 1)
        {
            printf("Enter Rental ID: ");
            int id;
            if (scanf("%d", &id) != 1)
            {
                flushInput();
                continue;
            }
            flushInput();

            int idx = -1;
            for (int i = 0; i < rentalCount; i++)
                if (rentals[i].id == id)
                    idx = i;

            if (idx == -1)
            {
                printf("Receipt not found.\n");
                pressEnterToContinue();
                continue;
            }

            printReceiptFromRental(&rentals[idx]);
            pressEnterToContinue();
        }
        else if (ch == 2)
        {
            reprintReceiptMenu();
        }
        else if (ch == 3)
        {
            break;
        }
        else
        {
            printf("Invalid choice.\n");
            pressEnterToContinue();
        }

    } while (1);
}

/* REPORTS FUNCTIONS */

void reportInventory()
{
    clearScreen();
    printf("\n========== INVENTORY REPORT ==========\n\n");

    int totalUnits = 0;
    int totalCars = carCount;

    for (int i = 0; i < carCount; i++)
        totalUnits += inventory[i].available;

    printf("Total Car Types: %d\n", totalCars);
    printf("Total Available Units: %d\n\n", totalUnits);

    printf("%-12s %-25s %-12s %-10s\n", "Plate", "Model", "Rate/Day", "Available");
    printf("--------------------------------------------------------------\n");

    for (int i = 0; i < carCount; i++)
        printf("%-12s %-25s PHP %-8.2f %-10d\n",
               inventory[i].plateNumber,
               inventory[i].model,
               inventory[i].pricePerDay,
               inventory[i].available);

    printf("--------------------------------------------------------------\n");
}

void reportCustomers()
{
    clearScreen();
    printf("\n========== CUSTOMER REPORT ==========\n\n");

    printf("Total Customers: %d\n\n", customerCount);

    printf("%-6s %-25s %-18s %-18s\n", "ID", "Name", "License", "Contact");
    printf("--------------------------------------------------------------\n");

    for (int i = 0; i < customerCount; i++)
        printf("%-6d %-25s %-18s %-18s\n",
               customers[i].id, customers[i].name,
               customers[i].licenseNo, customers[i].contact);

    printf("--------------------------------------------------------------\n");
}

void reportRentals()
{
    clearScreen();
    printf("\n========== RENTAL REPORT ==========\n\n");

    printf("Total Rental Records: %d\n\n", rentalCount);

    printf("%-6s %-20s %-22s %-8s %-6s %-10s %-8s\n",
           "ID", "Customer", "Car", "Qty", "Days", "Amount", "Ret");

    printf("--------------------------------------------------------------------------------------\n");

    for (int i = 0; i < rentalCount; i++)
        printf("%-6d %-20s %-22s %-8d %-6d PHP %-8.2f %-8s\n",
               rentals[i].id, rentals[i].customerName, rentals[i].model,
               rentals[i].quantity, rentals[i].days,
               rentals[i].totalAmount,
               rentals[i].returned ? "Yes" : "No");

    printf("--------------------------------------------------------------------------------------\n");
}

void reportsMenu()
{
    int ch;

    do
    {
        clearScreen();
        setColor(CLR_BLUE);
        printf("========================================\n");
        printf("               REPORTS MENU\n");
        printf("========================================\n");
        resetColor();

        printf("1. Inventory Report\n");
        printf("2. Customer Report\n");
        printf("3. Rental Report\n");
        printf("4. Back\n");
        printf("Choice: ");

        if (scanf("%d", &ch) != 1)
        {
            flushInput();
            continue;
        }
        flushInput();

        if (ch == 1)
        {
            reportInventory();
            pressEnterToContinue();
        }
        else if (ch == 2)
        {
            reportCustomers();
            pressEnterToContinue();
        }
        else if (ch == 3)
        {
            reportRentals();
            pressEnterToContinue();
        }
        else if (ch == 4)
            break;
        else
        {
            printf("Invalid choice.\n");
            pressEnterToContinue();
        }

    } while (1);
}

/* SETTINGS FUNCTIONS */

void changePassword(const char *currentUser)
{
    int idx = findUserByUsername(currentUser);
    if (idx == -1)
        return;

    char oldpass[PASSWORD_LEN], newpass[PASSWORD_LEN];

    printf("Enter Current Password: ");
    safeInput(oldpass, PASSWORD_LEN, NULL);

    if (strcmp(oldpass, users[idx].password) != 0)
    {
        printf("Incorrect password.\n");
        pressEnterToContinue();
        return;
    }

    printf("Enter New Password: ");
    safeInput(newpass, PASSWORD_LEN, NULL);

    strncpy(users[idx].password, newpass, PASSWORD_LEN - 1);
    saveUsers();

    setColor(CLR_GREEN);
    printf("Password changed successfully!\n");
    resetColor();
    pressEnterToContinue();
}

void settingsMenu(const char *currentUser)
{
    int ch;

    do
    {
        clearScreen();
        setColor(CLR_BLUE);
        printf("========================================\n");
        printf("             SETTINGS MENU\n");
        printf("========================================\n");
        resetColor();

        int idx = findUserByUsername(currentUser);
        int curIsAdmin = (idx != -1) ? users[idx].isAdmin : 0;

        printf("1. Change Password\n");
        if (curIsAdmin)
            printf("2. Manage Users\n3. Back\n");
        else
            printf("2. Back\n");
        printf("Choice: ");

        if (scanf("%d", &ch) != 1)
        {
            flushInput();
            continue;
        }
        flushInput();

        if (ch == 1)
            changePassword(currentUser);
        else if (curIsAdmin && ch == 2)
            userManagementMenu(currentUser);
        else if ((curIsAdmin && ch == 3) || (!curIsAdmin && ch == 2))
            break;
        else
        {
            printf("Invalid choice.\n");
            pressEnterToContinue();
        }

    } while (1);
}

/* USER MANAGEMENT */

void userManagementMenu(const char *currentUser)
{
    (void)currentUser;
    int choice;
    do
    {
        clearScreen();
        setColor(CLR_BLUE);
        printf("========================================\n");
        printf("           USER MANAGEMENT\n");
        printf("========================================\n");
        resetColor();

        printf("1. Add New User\n");
        printf("2. View Users\n");
        printf("3. Delete User\n");
        printf("4. Change User Password\n");
        printf("5. Toggle Admin Role\n");
        printf("0. Back\n");
        printf("Enter choice: ");

        if (scanf("%d", &choice) != 1)
        {
            flushInput();
            continue;
        }
        flushInput();

        switch (choice)
        {
        case 1:
            addUserMenu();
            break;
        case 2:
            viewUsersMenu();
            break;
        case 3:
            deleteUserMenu();
            break;
        case 4:
            changeUserPasswordMenu();
            break;
        case 5:
            toggleAdminMenu();
            break;
        case 0:
        default:
            break;
        }
    } while (choice != 0);
}

void addUserMenu()
{
    if (userCount >= MAX_USERS)
    {
        setColor(CLR_YELLOW);
        printf("User limit reached. Cannot add more users.\n");
        resetColor();
        pressEnterToContinue();
        return;
    }

    User u = {0};
    safeInput(u.username, USERNAME_LEN, "Username: ");
    if (findUserByUsername(u.username) != -1)
    {
        setColor(CLR_YELLOW);
        printf("Username already exists.\n");
        resetColor();
        pressEnterToContinue();
        return;
    }
    safeInput(u.password, PASSWORD_LEN, "Password: ");

    char rolebuf[8];
    safeInput(rolebuf, sizeof(rolebuf), "Role (1=Admin,0=Cashier): ");
    u.isAdmin = atoi(rolebuf) ? 1 : 0;

    u.id = nextUserId++;
    users[userCount++] = u;
    saveUsers();

    setColor(CLR_GREEN);
    printf("\nUser created successfully!\n");
    resetColor();
    pressEnterToContinue();
}

void viewUsersMenu()
{
    clearScreen();
    printf("=== Registered Users ===\n\n");
    for (int i = 0; i < userCount; i++)
    {
        printf("%d. %s  (%s)\n",
               users[i].id,
               users[i].username,
               users[i].isAdmin ? "ADMIN" : "CASHIER");
    }
    pressEnterToContinue();
}

int selectUserMenu()
{
    if (userCount == 0)
    {
        printf("No users found.\n");
        pressEnterToContinue();
        return -1;
    }

    printf("\n==== USERS ====\n");
    printf("%-5s %-15s %-6s\n", "No.", "Username", "Admin");
    printf("--------------------------\n");

    for (int i = 0; i < userCount; i++)
    {
        printf("%-5d %-15s %-6s\n",
               i + 1,
               users[i].username,
               users[i].isAdmin ? "YES" : "NO");
    }

    printf("\nSelect user (0 to cancel): ");
    int choice;
    if (scanf("%d", &choice) != 1)
    {
        flushInput();
        return -1;
    }
    flushInput();

    if (choice <= 0 || choice > userCount)
        return -1;

    return choice - 1;
}

void deleteUserMenu()
{
    int idx = selectUserMenu();
    if (idx == -1)
        return;

    printf("Delete user %s? (y/n): ", users[idx].username);
    char c = getchar();
    flushInput();

    if (c == 'y' || c == 'Y')
    {
        for (int i = idx; i < userCount - 1; i++)
            users[i] = users[i + 1];

        userCount--;
        saveUsers();
        printf("User deleted.\n");
    }

    pressEnterToContinue();
}

void changeUserPasswordMenu()
{
    int idx = selectUserMenu();
    if (idx == -1)
        return;

    char pass[PASSWORD_LEN];
    safeInput(pass, PASSWORD_LEN, "Enter new password: ");

    strncpy(users[idx].password, pass, PASSWORD_LEN - 1);
    saveUsers();

    printf("Password updated.\n");
    pressEnterToContinue();
}

void toggleAdminMenu()
{
    int idx = selectUserMenu();
    if (idx == -1)
        return;

    users[idx].isAdmin = !users[idx].isAdmin;
    saveUsers();

    printf("Admin role updated.\n");
    pressEnterToContinue();
}

/* MAIN DASHBOARD */

void mainDashboard(const char *loggedUser, int isAdmin)
{
    int choice;
    do
    {
        clearScreen();
        setColor(CLR_BLUE);
        printf("========================================\n");
        printf("         FOURSIGHT CAR RENTALS\n");
        printf("========================================\n");
        resetColor();

        printf("Logged in as: %s\n\n", loggedUser);

        int totalAvailable = 0, active = 0;
        for (int i = 0; i < carCount; i++)
            totalAvailable += inventory[i].available;
        for (int i = 0; i < rentalCount; i++)
            if (!rentals[i].returned)
                active++;

        printf("Total car types: %d | Available units: %d | Active rentals: %d | Customers: %d\n\n",
               carCount, totalAvailable, active, customerCount);

        printf("1. Car Inventory\n");
        printf("2. Customers\n");
        printf("3. Rent a Car\n");
        printf("4. Return a Car\n");
        printf("5. Payment / Billing\n");
        printf("6. Reports\n");

        if (isAdmin)
            printf("7. Settings\n8. Logout\nChoice: ");
        else
            printf("7. Logout\nChoice: ");

        if (scanf("%d", &choice) != 1)
        {
            flushInput();
            printf("Invalid input.\n");
            pressEnterToContinue();
            continue;
        }
        flushInput();

        if (isAdmin)
        {
            if (choice == 1)
                viewInventoryInteractive();
            else if (choice == 2)
                customerMenu();
            else if (choice == 3)
                rentCarMenu(loggedUser);
            else if (choice == 4)
                returnCar();
            else if (choice == 5)
                paymentBillingMenu();
            else if (choice == 6)
                reportsMenu();
            else if (choice == 7)
                settingsMenu(loggedUser);
            else if (choice == 8)
                break;
            else
            {
                printf("Invalid choice.\n");
                pressEnterToContinue();
            }
        }
        else
        {
            if (choice == 1)
                viewInventoryInteractive();
            else if (choice == 2)
                customerMenu();
            else if (choice == 3)
                rentCarMenu(loggedUser);
            else if (choice == 4)
                returnCar();
            else if (choice == 5)
                paymentBillingMenu();
            else if (choice == 6)
                reportsMenu();
            else if (choice == 7)
                break;
            else
            {
                printf("Invalid choice.\n");
                pressEnterToContinue();
            }
        }
    } while (1);
}

/*  MAIN FUNCTION */

int main()
{
    initializeSystem();

    while (1)
    {
        char loggedUser[USERNAME_LEN] = {0};
        int isAdmin = 0;

        int status = login(loggedUser, &isAdmin);

        if (status == -1)
        {
            printf("Exiting program...\n");
            break;
        }
        else if (status == 0)
        {
            setColor(CLR_RED);
            printf("Login Failed.\n");
            resetColor();
            pressEnterToContinue();
        }
        else
        {
            mainDashboard(loggedUser, isAdmin);
        }
    }

    curl_global_cleanup();
    return 0;
}
