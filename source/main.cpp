#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <fstream>

#include <switch.h>

//This example shows how to access savedata for (official) applications/games.

const char * EXPORT_DIR = "smm2 courses/";
const char * INJECT_DIR = "inject/";
const char * SAVE_DEV = "save";

Result get_save(u64 *titleID, u128 *userID)
{
    Result rc=0;
    FsSaveDataIterator iterator;
    size_t total_entries=0;
    FsSaveDataInfo info;

    rc = fsOpenSaveDataIterator(&iterator, FsSaveDataSpaceId_NandUser);//See libnx fs.h.
    if (R_FAILED(rc)) {
        printf("fsOpenSaveDataIterator() failed: 0x%x\n", rc);
        return rc;
    }

    //Find the first savedata with FsSaveDataType_SaveData.
    while(1) {
        rc = fsSaveDataIteratorRead(&iterator, &info, 1, &total_entries);//See libnx fs.h.
        if (R_FAILED(rc) || total_entries==0) break;

        if (info.SaveDataType == FsSaveDataType_SaveData) {//Filter by FsSaveDataType_SaveData, however note that NandUser can have non-FsSaveDataType_SaveData.
            *titleID = 0x01009b90006dc000;
            *userID = info.userID;
            return 0;
        }
    }

    fsSaveDataIteratorClose(&iterator);

    if (R_SUCCEEDED(rc)) return MAKERESULT(Module_Libnx, LibnxError_NotFound);

    return rc;
}
int isDirectory(const char *path) {
   struct stat statbuf;
   if (stat(path, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}
int cpFile(const char * filenameI, const char * filenameO) {
    remove( filenameO );
    
    std::ifstream src(filenameI, std::ios::binary);
    std::ofstream dst(filenameO, std::ios::binary);

    dst << src.rdbuf();

    return 0;
}

int copyAllSave(const char * dev, const char * path, bool isInject, 
    const char * exportDir) {
    DIR* dir;
    struct dirent* ent;
    char dirPath[0x100];
    if(isInject) {
        strcpy(dirPath, EXPORT_DIR);
        strcat(dirPath, path);
    } else {                    
        strcpy(dirPath, dev);
        strcat(dirPath, path);
    }

    dir = opendir(dirPath);
    if(dir==NULL)
    {
        printf("Failed to open dir: %s\n", dirPath);
        return -1;
    }
    else
    {
        while ((ent = readdir(dir)))
        {

            char filename[0x100];
            strcpy(filename, path);
            strcat(filename, "/");
            strcat(filename, ent->d_name);

            char filenameI[0x100];
            char filenameO[0x100];
            if(isInject) {
                strcpy(filenameI, EXPORT_DIR);
                strcat(filenameI, filename);

                strcpy(filenameO, dev);
                strcat(filenameO, filename);
            } else {
                strcpy(filenameI, dev);
                strcat(filenameI, filename);

                if (exportDir != NULL) {
                    strcpy(filenameO, exportDir);
                } else {
                    strcpy(filenameO, EXPORT_DIR);
                }
                strcat(filenameO, filename);
            }

            if(isDirectory(filenameI))
			{
                mkdir(filenameO, 0700);
                int res = copyAllSave(dev, filename, isInject, exportDir);
                if(res != 0)
                    return res;
			}
			else {
                printf("Copying %s... ", filenameI);
                cpFile(filenameI, filenameO);
                if(isInject) {
                    if (R_SUCCEEDED(fsdevCommitDevice(SAVE_DEV))) { // Thx yellows8
                        printf("committed.\n");
                    } else {
                        printf("fsdevCommitDevice() failed...\n");
                        return -2;
                    }
                } else {
                    printf("\n");
                }
            }
        }
        closedir(dir);
        return 0;
    }
}

int inject() {
    return copyAllSave("save:/", ".", true, NULL);
}

int main(int argc, char **argv)
{
    Result rc=0;
    int ret=0;

    DIR* dir;
	DIR* dir2;
    struct dirent* ent;

    FsFileSystem tmpfs;
    u128 userID=0;
    bool account_selected=0;
    u64 titleID=0x01009b90006dc000;//SMM2 TitleID

    gfxInitDefault();
    consoleInit(NULL);

    //Get the userID for save mounting. To mount common savedata, use FS_SAVEDATA_USERID_COMMONSAVE.

    //Try to find savedata to use with get_save() first, otherwise fallback to the above hard-coded TID + the userID from accountGetActiveUser(). Note that you can use either method.
    //See the account example for getting account info for an userID.
    //See also the app_controldata example for getting info for a titleID.
    if (R_FAILED(get_save(&titleID, &userID))) {
        rc = accountInitialize();
        if (R_FAILED(rc)) {
            printf("accountInitialize() failed: 0x%x\n", rc);
        }

        if (R_SUCCEEDED(rc)) {
            rc = accountGetActiveUser(&userID, &account_selected);
            accountExit();

            if (R_FAILED(rc)) {
                printf("accountGetActiveUser() failed: 0x%x\n", rc);
            }
            else if(!account_selected) {
                printf("No user is currently selected.\n");
                rc = -1;
            }
        }
    }

    if (R_SUCCEEDED(rc)) {
        printf("Welcome to the SMM2 Course Injection Tool! Commencing injection process...\n");
    }

    if (R_SUCCEEDED(rc)) {
        rc = fsMount_SaveData(&tmpfs, titleID, userID);//See also libnx fs.h.
        if (R_FAILED(rc)) {
            printf("fsMount_SaveData() failed: 0x%x\n", rc);
        }
    }

    //You can use any device-name. If you want multiple saves mounted at the same time, you must use different device-names for each one.
    if (R_SUCCEEDED(rc)) {
        ret = fsdevMountDevice("save", tmpfs);
        if (ret==-1) {
            printf("fsdevMountDevice() failed.\n");
            rc = ret;
        }
    }

    //At this point you can use the mounted device with standard stdio.
    //After modifying savedata, in order for the changes to take affect you must use: rc = fsdevCommitDevice("save");
    //See also libnx fs_dev.h for fsdevCommitDevice.

    if (R_SUCCEEDED(rc))
	{
		rc = fsdevMountSdmc();
		dir2 = opendir("sdmc:/");
        dir = opendir("save:/");//Open the "save:/" directory.
        if(dir==NULL)
        {
            printf("Failed to open dir.\n");
        }
        else
        {
            closedir(dir);
        }
		inject();
		printf("Injection completed!\n");
		rc = fsdevCommitDevice("save");
		printf("Press the '+' button to exit application\n");
		printf("This tool was created by Chrs2324");

        //When you are done with savedata, you can use the below.
        //Any devices still mounted at app exit are automatically unmounted.
        fsdevUnmountDevice("save");
    }

    // Main loop
    while(appletMainLoop())
    {
        //Scan all the inputs. This should be done once for each frame
        hidScanInput();

        //hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
        u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);

        if (kDown & KEY_PLUS) break; // break in order to return to hbmenu

        gfxFlushBuffers();
        gfxSwapBuffers();
        gfxWaitForVsync();
    }

    gfxExit();
    return 0;
}

