#include <QCoreApplication>
#include <windows.h>
#include <qdebug.h>
#include <setupapi.h>
#include <winusb.h>
#include <winioctl.h>
#include <devguid.h>
#include <cfgmgr32.h>

QString getDriveInfo(QString & tdrive)
{
    WCHAR szVolumeName[256] ;
    WCHAR szFileSystemName[256];
    DWORD dwSerialNumber = 0;
    DWORD dwMaxFileNameLength=256;
    DWORD dwFileSystemFlags=0;

    bool ret = false;
    qDebug()<< "Flag:" <<GetVolumeInformation( (WCHAR *) tdrive.utf16(),szVolumeName,256,&dwSerialNumber,&dwMaxFileNameLength,&dwFileSystemFlags,szFileSystemName,256);
    qDebug()<< "Drive:" << tdrive;
    qDebug()<< "FormatType:"<<QString::fromUtf16 ( (const ushort *) szFileSystemName) ;
    qDebug()<< "VolumeName:"<<QString::fromUtf16 ( (const ushort *) szVolumeName);
    qDebug()<< "SerialNumber:"<<dwSerialNumber ;
    qDebug()<< "FileSystemFlags:"<< dwFileSystemFlags;
    if(!ret)return QString("");
    QString vName=QString::fromUtf16 ( (const ushort *) szVolumeName) ;
    vName.trimmed();
    return vName;
}

/**
 * @brief 获取所有usb设备的信息
 * @return
 */
int getALLUsb(){
    HDEVINFO deviceInfoSet;
    GUID *guidDev = (GUID*) &GUID_DEVCLASS_USB;
    deviceInfoSet = SetupDiGetClassDevs(guidDev, NULL, NULL, DIGCF_PRESENT | DIGCF_PROFILE);
    TCHAR buffer [1024];
    int memberIndex = 0;
    while (true) {
        SP_DEVINFO_DATA deviceInfoData;
        ZeroMemory(&deviceInfoData, sizeof(SP_DEVINFO_DATA));
        deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        if (SetupDiEnumDeviceInfo(deviceInfoSet, memberIndex, &deviceInfoData) == FALSE) {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) {
                break;
            }
        }

        DWORD nSize=0 ;
        SetupDiGetDeviceInstanceId (deviceInfoSet, &deviceInfoData, buffer, sizeof(buffer), &nSize);
        buffer [nSize] ='\0';

        QString info = QString::fromUtf16((ushort*)buffer);
        qDebug()<<"getALL:"<< info;
        memberIndex++;
    }
    if (deviceInfoSet) {
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
    }

    return 0;
}

/**
 * @brief 获取盘符
 * @param deviceHandle
 * @return
 */
DWORD getDeviceNumber( HANDLE deviceHandle )
{
    STORAGE_DEVICE_NUMBER sdn;
    sdn.DeviceNumber = -1;
    DWORD dwBytesReturned = 0;
    if ( !DeviceIoControl( deviceHandle,
                           IOCTL_STORAGE_GET_DEVICE_NUMBER,
                           NULL, 0, &sdn, sizeof( sdn ),
                           &dwBytesReturned, NULL ) )
    {
        // handle error - like a bad handle.
        qDebug()<<"get volumeNumber error";
        return -1;
    }
    return sdn.DeviceNumber;
}

/**
 * @brief 获取磁盘的pid、vid
 * @param device
 * @return
 */
QString getDeviceID(DEVINST device)
{
    QString deviceIDStr = "";
    // Get a list of hardware IDs for all USB devices.
    ULONG ulLen;
    CM_Get_Device_ID_List_Size(&ulLen, NULL, CM_GETIDLIST_FILTER_NONE);
    TCHAR *pszBuffer = new TCHAR[ulLen];
    CM_Get_Device_ID_List(NULL, pszBuffer, ulLen, CM_GETIDLIST_FILTER_NONE);

    // Iterate through the list looking for our ID.
    for(LPTSTR pszDeviceID = pszBuffer; *pszDeviceID; pszDeviceID += _tcslen(pszDeviceID) + 1)
    {
        // Some versions of Windows have the string in upper case and other versions have it
        // in lower case so just make it all upper.
        for(int i = 0; pszDeviceID[i]; i++)
            pszDeviceID[i] = toupper(pszDeviceID[i]);

        //if(_tcsstr(pszDeviceID, hwid))
        //{
        // Found the device, now we want the grandchild device, which is the "generic volume"
        DEVINST MSDInst = 0;
        if(CR_SUCCESS == CM_Locate_DevNode(&MSDInst, pszDeviceID, CM_LOCATE_DEVNODE_NORMAL))
        {
            DEVINST DiskDriveInst = 0;
            if(CR_SUCCESS == CM_Get_Child(&DiskDriveInst, MSDInst, 0))
            {
                // Now compare the grandchild node against the given device instance.
                if(device == DiskDriveInst)
                {
                    deviceIDStr = QString::fromWCharArray(pszDeviceID);
                }
            }
        }
        //}
    }
    return deviceIDStr;
}

/**
 * @brief 根据盘符获取设备信息
 * @param drive
 */
QString getDeviceInfo(QString letter)
{
    QString drive ="\\\\.\\";
    drive.append(letter);

    HANDLE deviceHandle = CreateFile( (WCHAR *)drive.utf16(),
                                      GENERIC_READ | GENERIC_WRITE,FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      NULL,
                                      OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL,
                                      NULL);
    if ( deviceHandle == INVALID_HANDLE_VALUE ){
        return "";
    }

    // to get the device number
    DWORD volumeDeviceNumber = getDeviceNumber( deviceHandle );
    //qDebug()<<"volumeNumber:"<<volumeDeviceNumber;
    CloseHandle( deviceHandle );

    // Get device interface info set handle
    // for all devices attached to the system
    HDEVINFO hDevInfo = SetupDiGetClassDevs(
                &GUID_DEVINTERFACE_DISK, NULL, NULL,
                DIGCF_PRESENT | DIGCF_DEVICEINTERFACE );

    if ( hDevInfo == INVALID_HANDLE_VALUE ){
        return "";
    }

    // Get a context structure for the device interface
    // of a device information set.
    BYTE Buf[1024];
    PSP_DEVICE_INTERFACE_DETAIL_DATA pspdidd = (PSP_DEVICE_INTERFACE_DETAIL_DATA)Buf;
    SP_DEVICE_INTERFACE_DATA         spdid;
    SP_DEVINFO_DATA                  spdd;

    spdid.cbSize = sizeof( spdid );

    DWORD dwIndex = 0;
    while ( true )
    {
        if ( ! SetupDiEnumDeviceInterfaces( hDevInfo, NULL,
                                            &GUID_DEVINTERFACE_DISK,
                                            dwIndex, &spdid ))
        {
            break;
        }

        DWORD dwSize = 0;
        SetupDiGetDeviceInterfaceDetail( hDevInfo, &spdid, NULL,
                                         0, &dwSize, NULL );

        if (( dwSize != 0 ) && ( dwSize <= sizeof( Buf )))
        {
            pspdidd->cbSize = sizeof( *pspdidd ); // 5 Bytes!

            ZeroMemory((PVOID)&spdd, sizeof(spdd));
            spdd.cbSize = sizeof(spdd);

            long res = SetupDiGetDeviceInterfaceDetail(
                        hDevInfo, &spdid, pspdidd,
                        dwSize, &dwSize, &spdd );
            if ( res )
            {
                HANDLE hDrive = CreateFile( pspdidd->DevicePath,0,
                                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                                            NULL, OPEN_EXISTING, 0, NULL );
                if ( hDrive != INVALID_HANDLE_VALUE )
                {
                    DWORD usbDeviceNumber = getDeviceNumber( hDrive );

                    if ( usbDeviceNumber == volumeDeviceNumber )
                    {

                        QString info = QString::fromUtf16((ushort*)pspdidd->DevicePath);
                        qInfo()<< "sn:" << info;

                        QString deviceID = getDeviceID(spdd.DevInst);
                        qInfo()<<"deviceid:"<<deviceID;
                    }
                }
                CloseHandle( hDrive );
            }
        }
        dwIndex++;
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return "";
}

void getAllDevices(){
    // 获取当前系统所有使用的设备
    DWORD dwFlag = (DIGCF_ALLCLASSES | DIGCF_PRESENT);
    HDEVINFO hDevInfo = SetupDiGetClassDevs(NULL, NULL, NULL, dwFlag);
    if( INVALID_HANDLE_VALUE == hDevInfo )
    {
        qDebug()<<_T("获取系统设备列表失败");
        return;
    }

    // 准备遍历所有设备查找USB
    SP_DEVINFO_DATA sDevInfoData;
    sDevInfoData.cbSize = sizeof(SP_DEVINFO_DATA);


    //CString strText;
    TCHAR szDIS[MAX_PATH]; // Device Identification Strings,
    DWORD nSize = 0 ;
    for(int i = 0; SetupDiEnumDeviceInfo(hDevInfo,i,&sDevInfoData); i++ )
    {
        nSize = 0;
        if ( !SetupDiGetDeviceInstanceId(hDevInfo, &sDevInfoData, szDIS, sizeof(szDIS), &nSize) )
        {
            qDebug()<<( _T("获取设备识别字符串失败") );
            break;
        }else{
            QString info = QString::fromUtf16((ushort*)szDIS);
            //if(info.startsWith("USBSTOR")){
            qDebug()<<"getusb:"<<info;
            //}
        }
    }

    // 释放设备
    SetupDiDestroyDeviceInfoList(hDevInfo);
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QString path = QCoreApplication::applicationDirPath();
    path = path.left(path.indexOf("/"));
    getDeviceInfo(path);
    return 0;
    //return a.exec();
}
