/**
* This file has been modified from its orginal sources.
*
* Copyright (c) 2012 Software in the Public Interest Inc (SPI)
* Copyright (c) 2012 David Pratt
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
***
* Copyright (c) 2008-2012 Appcelerator Inc.
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#include "../utils.h"

#ifdef OS_OSX
#include <Cocoa/Cocoa.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IOEthernetController.h>
#include <sys/utsname.h>
#include <libgen.h>
#elif defined(OS_LINUX)
#include <cstdarg>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/utsname.h>
#include <libgen.h>
#endif

#include <iostream>
#include <sstream>
#include <cstring>

namespace UTILS_NS
{
    std::string FileUtils::GetExecutableDirectory()
    {
#ifdef OS_OSX
        NSString* bundlePath = [[NSBundle mainBundle] bundlePath];
        NSString* contents = [NSString stringWithFormat:@"%@/Contents",bundlePath];
        return std::string([contents UTF8String]);
#elif OS_LINUX
        char tmp[100];
        sprintf(tmp,"/proc/%d/exe",getpid());
        char pbuf[255];
        int c = readlink(tmp,pbuf,255);
        pbuf[c]='\0';
        std::string str(pbuf);
        size_t pos = str.rfind("/");
        if (pos==std::string::npos) return str;
        return str.substr(0,pos);
#endif
    }

    std::string FileUtils::Dirname(const std::string& path)
    {
        char* pathCopy = strdup(path.c_str());
        std::string toReturn = dirname(pathCopy);
        free(pathCopy);
        return toReturn;
    }

    std::string FileUtils::GetTempDirectory()
    {
#ifdef OS_OSX
        NSString * tempDir = NSTemporaryDirectory();
        if (tempDir == nil)
            tempDir = @"/tmp";

        NSString *tmp = [tempDir stringByAppendingPathComponent:@"kXXXXX"];
        const char * fsTemplate = [tmp fileSystemRepresentation];
        NSMutableData * bufferData =
            [NSMutableData dataWithBytes: fsTemplate length: strlen(fsTemplate)+1];
        char * buffer = (char*)[bufferData mutableBytes];
        mkdtemp(buffer);
        NSString * temporaryDirectory = [[NSFileManager defaultManager]
            stringWithFileSystemRepresentation: buffer
            length: strlen(buffer)];
        return std::string([temporaryDirectory UTF8String]);
#else
        std::ostringstream dir;
        const char* tmp = getenv("TMPDIR");
        const char* tmp2 = getenv("TEMP");
        if (tmp)
            dir << std::string(tmp);
        else if (tmp2)
            dir << std::string(tmp2);
        else
            dir << std::string("/tmp");

        std::string tmp_str = dir.str();
        if (tmp_str.at(tmp_str.length()-1) != '/')
            dir << "/";
        dir << "kXXXXXX";
        char* tempdir = strdup(dir.str().c_str());
        tempdir = mkdtemp(tempdir);
        tmp_str = std::string(tempdir);
        free(tempdir);
        return tmp_str;
#endif
    }

    bool FileUtils::IsFile(const std::string& file)
    {
#ifdef OS_OSX
        BOOL isDir = NO;
        NSString *f = [NSString stringWithCString:file.c_str()  encoding:NSUTF8StringEncoding];
        NSString *p = [f stringByStandardizingPath];
        BOOL found = [[NSFileManager defaultManager] fileExistsAtPath:p isDirectory:&isDir];
        return found && !isDir;
#elif OS_LINUX
        struct stat st;
        return (stat(file.c_str(),&st)==0) && S_ISREG(st.st_mode);
#endif
    }

    bool FileUtils::CreateDirectoryImpl(const std::string& dir)
    {
#ifdef OS_OSX
        NSFileManager* manager = [NSFileManager defaultManager];
        NSString* path = [NSString stringWithCString:dir.c_str() encoding:NSUTF8StringEncoding];
        return [manager createDirectoryAtPath:path withIntermediateDirectories:NO attributes:nil error:nil];
#elif OS_LINUX
        return mkdir(dir.c_str(),0755) == 0;
#endif
    }

    bool FileUtils::DeleteDirectory(const std::string& dir)
    {
#ifdef OS_OSX
        return [[NSFileManager defaultManager] removeItemAtPath:[NSString stringWithCString:dir.c_str() encoding:NSUTF8StringEncoding] error:nil];
#elif OS_LINUX
        return unlink(dir.c_str()) == 0;
#endif
        return false;
    }

    bool FileUtils::DeleteFile(const std::string& file)
    {
        return unlink(file.c_str()) == 0;
    }

    bool FileUtils::IsDirectory(const std::string& dir)
    {
#ifdef OS_OSX
        BOOL isDir = NO;
        BOOL found = [[NSFileManager defaultManager] fileExistsAtPath:[NSString stringWithCString:dir.c_str() encoding:NSUTF8StringEncoding] isDirectory:&isDir];
        return found && isDir;
#elif OS_LINUX
        struct stat st;
        return (stat(dir.c_str(),&st)==0) && S_ISDIR(st.st_mode);
#endif
    }

    bool FileUtils::IsHidden(const std::string& file)
    {
        // TODO: OS X can also include a 'hidden' flag in file
        // attributes. We should attempt to read this.
        return (file.size() > 0 && file.at(0) == '.');
    }

    void FileUtils::ListDir(const std::string& path, std::vector<std::string> &files)
    {
        if (!IsDirectory(path))
            return;
        files.clear();

        DIR *dp;
        struct dirent *dirp;
        if ((dp = opendir(path.c_str()))!=NULL)
        {
            while ((dirp = readdir(dp))!=NULL)
            {
                std::string fn = std::string(dirp->d_name);
                if (fn.substr(0,1)=="." || fn.substr(0,2)=="..")
                {
                    continue;
                }
                files.push_back(fn);
            }
            closedir(dp);
        }
    }

    int FileUtils::RunAndWait(const std::string& path, std::vector<std::string> &args)
    {
        std::string p;
        p+="\"";
        p+=path;
        p+="\" ";
        std::vector<std::string>::iterator i = args.begin();
        while (i!=args.end())
        {
            p+="\"";
            p+=(*i++);
            p+="\" ";
        }

#ifdef DEBUG
        std::cout << "running: " << p << std::endl;
#endif
        int status = system(p.c_str());
        return WEXITSTATUS(status);
    }

#ifndef NO_UNZIP
    bool FileUtils::Unzip(const std::string& source, const std::string& destination, 
        UnzipCallback callback, void* data)
    {
#ifdef OS_OSX
        //
        // we don't include gzip since we're on OSX
        // we just let the built-in OS handle extraction for us
        //
        std::vector<std::string> args;
        args.push_back("--noqtn");
        args.push_back("-x");
        args.push_back("-k");
        args.push_back("--rsrc");
        args.push_back(source);
        args.push_back(destination);
        std::string cmdline = "/usr/bin/ditto";
        RunAndWait(cmdline, args);
        return true;
#elif OS_LINUX
        std::vector<std::string> args;
        args.push_back("-qq");
        args.push_back("-o");
        args.push_back(source);
        args.push_back("-d");
        args.push_back(destination);
        std::string cmdline("/usr/bin/unzip");
        RunAndWait(cmdline, args);
        return true;
#endif
    }
#endif
}