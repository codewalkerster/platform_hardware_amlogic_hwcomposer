/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <pthread.h>
#include <sys/time.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>

#include "DisplayAdapter.h"

#ifndef NOTIMPLEMENTED
#define NOTIMPLEMENTED fprintf(stderr, "Function:%s not implemented", __PRETTY_FUNCTION__)
#endif

using meson::DisplayAdapter;
using std::unique_ptr;

static const char* short_option = "c:g:s:r:G:S:F:w:f:h:P:p:R:t:m:v";

static const struct option long_option[] = {
    {"chang-mode", required_argument, 0, 'c'},
    {"get-property", required_argument, 0, 'g'},
    {"set-property", required_argument, 0, 's'},
    {"raw-cmd", required_argument, 0, 'r'},
    {"sync-protection",required_argument,0,'P'},
    {"disable-sideband",required_argument,0,'p'},
    {"change-type", required_argument, 0, 't'},
    {"perferred-mode", required_argument, 0, 'm'},
    {"keystone-configs",required_argument,0,'k'},
    {"reverse-display",required_argument,0,'V'},
    {"get-connector-type",required_argument,0,'n'},
    {"disable-qms",required_argument,0,'v'},
    {"get-connector-type",required_argument,0,'n'},
    {0, 0, 0, 0}
};

static void print_usage(const char* name) {
    printf("Usage: %s [-cgsGSFwfhPpRtmkVnr]\n"
            "Get or change the mode setting of the weston drm output.\n"
            "Options:\n"
            "       -c,--change-mode MODE  \tchange connector current mode, MODE format like:%%dx%%d@%%d width,height,refresh\n"
            "       -g,--get-display-attribute  \"ATTRI_NAME\"\tget display attribute\n"
            "       -s,--set-display-attribute  \"ATTRI_NAME\"=value\tset display attribute\n"
            "       -G \"[ui-rect|display-mode]\"\tget [logic ui rect|display mode]\n"
            "       -S \"[ui-rect]\"\tset [logic ui rect]\n"
            "                               \t eg: \"Content Protection\" 1\n"
            "       -F, [framerate] \tset framerate for AFR\n"
            "       -w,--set-whiteboard  \t  disable [false|true]white board \n "
            "       -f,--set-whiteboard display position \t  x and y is the position\n "
            "       -h,--hide the video layer \t  hide the video layer\n "
            "       -p,--disable-sideband  \t  disable [false|true]side band \n "
            "       -P,--sync Protection control \t  sync Protection control\n "
            "       -R,--set-hdr-conversionstrategy \tset [strategy] [fource type]\n"
            "       -t,--change-type TYPE  \tchange connector type, TYPE could reference DisplayAdapter::ConnectorType\n"
            "       -m,--perferred-mode MODE\tchange perferred mode, MODE format like:%%dx%%d@%%d width,height,refresh\n"
            "       -k,--set-Keystone Correction \t  params is the position\n "
            "       -V,--enable keystone reverse  \t  params is the reverse type\n "
            "       -v,--disable-qms        \t  disable [false|true] qms\n"
            "       -n,--get-connector-type \t  get connector type by display id\n "
            "       -r,--raw-cmd           \tsend raw cmd\n"
            "                              \teg: \"userSpaceHDCPTxAuth\" \n"
            "                                    \"getSupportDisplayModes\" \n"
            "                                    \"getDisplayIds\" \n"
            "                                    \"dumpDisplayAttribute\" \n"
            "                                    \"getCurrentSupportDeepColor\" \n"
            "                                    \"getWhiteBoardMode\" \n"
            "                                    \"isHdmiUsed\" \n"
            "                                    \"getHdrSdrRatio\" \n"
            "                                    \"getVideoPosition\" [display id] \n"
            "                              \teg: \"setDisplayConnector\" [connector type] TYPE could be like:\n"
            "                              \t    DUMMY\n"
            "                              \t    CVBS\n"
            "                              \t    HDMI, HDMI is default connector\n"
            "                              \teg: \"userSpaceHDCPTxAuth\" \n", name);
}

int main(int argc, char* argv[]) {
    std::vector<meson::DisplayModeInfo> displayModeList;
    if (argc == 1) {
        print_usage(argv[0]);
        return 0;
    }
#ifndef RECOVERY_MODE
    unique_ptr<DisplayAdapter> client = meson::DisplayAdapterCreateRemote();
    DEBUG_INFO("Start client");
#else
    unique_ptr<DisplayAdapter> client = meson::DisplayAdapterCreateLocal();
    DEBUG_INFO("Start recovery client");
#endif
    DisplayAdapter::ConnectorType type = DisplayAdapter::CONN_TYPE_HDMI;
    std::vector<int> displayIdList;

    int opt;

    while ((opt = getopt_long(argc, argv, short_option, long_option, NULL)) != -1) {
        switch (opt) {
             case 'n':
                if (optarg == NULL)
                    break;
                DisplayAdapter::ConnectorType result;
                if (client->getConnectorType(std::stoi(optarg), result)) {
                    printf("display id %d ConnectorType %d\n", std::stoi(optarg), result);
                } else {
                    printf("get connectorType failed\n");
                }
                break;
            case 'c':
                if (optarg == NULL)
                    break;
                client->setDisplayMode(optarg, type);
                break;
            case 'g':
                if (optarg == NULL)
                    break;
                {
                    std::string value;
                    client->getDisplayAttribute(optarg, value, type);
                    printf("%s\n", value.c_str());
                }
                break;
            case 's':
                if (optind + 1 > argc) {
                    printf("miss parameter\n");
                    break;
                }
                {
                    client->setDisplayAttribute(optarg, argv[optind], type);
                    printf("set %s to %s", optarg, argv[optind]);
                    std::string value;
                    client->getDisplayAttribute(optarg, value, type);
                    printf(", current value:%s\n", value.c_str());
                }
                optind++;
                break;
            case 'G':
                if (optarg == NULL)
                    break;
                if (0 == memcmp("display-mode", optarg, sizeof("display-mode"))) {
                    std::string mode;
                    client->getDisplayMode(mode, type);
                    printf("%s\n", mode.c_str());
                } else if (0 == memcmp("ui-rect", optarg, sizeof("ui-rect"))) {
                    meson::Rect rect;
                    client->getDisplayRect(rect, type);
                    printf("%s\n", rect.toString().c_str());
                } else {
                    NOTIMPLEMENTED;
                }
                break;
            case 'S':
                if (optarg == NULL)
                    break;
                {
                    if (0 == memcmp("display-mode", optarg, sizeof("display-mode"))) {
                        if (optind + 1 > argc) {
                            printf("miss parameter");
                            break;
                        }
                        client->setDisplayMode(argv[optind], type);
                        optind++;
                    } else if (0 == memcmp("ui-rect", optarg, sizeof("ui-rect"))) {
                        meson::Rect rect;
                        if (optind + 4 > argc) {
                            printf("miss rect parameter");
                            break;
                        }
                        rect.x = strtol(argv[optind], NULL, 10);
                        optind++;
                        rect.y = strtol(argv[optind], NULL, 10);
                        optind++;
                        rect.w = strtol(argv[optind], NULL, 10);
                        optind++;
                        rect.h = strtol(argv[optind], NULL, 10);
                        printf("set ui to (%s)", rect.toString().c_str());
                        client->setDisplayRect(rect, type);
                        optind++;
                    } else {
                        NOTIMPLEMENTED;
                    }
                }
                break;
            case 'r':
                if (optarg == NULL)
                    break;

                if (memcmp("getSupportDisplayModes", optarg, sizeof("getSupportDisplayModes")) == 0) {
                    if (client->getSupportDisplayModes(displayModeList, type)) {
                        for (auto const &mode : displayModeList) {
                            printf("%s %u %u %u %u %f \n", mode.name.c_str(), mode.dpiX, mode.dpiY, mode.pixelW, mode.pixelH, mode.refreshRate);
                        }
                    }
                } else if (memcmp("dumpDisplayAttribute", optarg, sizeof("dumpDisplayAttribute")) == 0) {
                    Json::Value json;
                    client->dumpDisplayAttribute(json, type);
                    printf("Dump display attribute:\n%s", meson::JsonValue2String(json).c_str());
                } else if (memcmp("getCurrentSupportDeepColor", optarg, sizeof("getCurrentSupportDeepColor")) == 0) {
                    std::string color;
                    client->getCurrentSupportDeepColor(color, type);
                    printf("current supported deepColor:%s\n", color.c_str());
                } else if (memcmp("getWhiteBoardMode", optarg, sizeof("getWhiteBoardMode")) == 0) {
                    bool mode = false;
                    client->getWhiteBoardMode(mode);
                    printf("get current white board %s \n", mode ? "true":"false");
                } else if (memcmp("getHdrSdrRatio", optarg, sizeof("getHdrSdrRatio")) == 0) {
                    float ratio;
                    client->getHdrSdrRatio(ratio, type);
                    printf("getHdrSdrRatio %f \n", ratio);
                } else if (memcmp("getVideoPosition", optarg, sizeof("getVideoPosition")) == 0) {
                    int left,top,right,bottom;
                    client->getVideoPosition(std::stoi(argv[optind]), left, top, right, bottom);
                    printf("display id %d video position [%d %d %d %d]\n", std::stoi(argv[optind]), left, top, right, bottom);
                } else if (memcmp("getQmsVrrCap", optarg, sizeof("getQmsVrrCap")) == 0) {
                    bool qms = client->getQmsVrrCap();
                    printf("Qms Vrr Cap :%d\n", qms);
                } else if (memcmp("setDisplayConnector", optarg, sizeof("setDisplayConnector")) == 0) {
                    if (optind + 1 > argc) {
                        printf("miss parameter");
                        break;
                    }

                    DisplayAdapter::ConnectorType connector = DisplayAdapter::CONN_TYPE_UNKNOWN;
                    if (strcmp(argv[optind], "CVBS") == 0) {
                        connector = DisplayAdapter::CONN_TYPE_CVBS;
                    } else if (strcmp(argv[optind], "DUMMY") == 0) {
                        connector = DisplayAdapter::CONN_TYPE_DUMMY;
                    } else if (strcmp(argv[optind], "HDMI") == 0) {
                        connector = DisplayAdapter::CONN_TYPE_HDMIA;
                    }

                    if (client->setFixedConnectorDisplay(connector)) {
                        printf("setFixedConnectorDisplay connector %s success\n", argv[optind]);
                    } else {
                        printf("setFixedConnectorDisplay connector %s failed\n", argv[optind]);
                    }
                    optind++;
                } else if (memcmp("userSpaceHDCPTxAuth", optarg, sizeof("userSpaceHDCPTxAuth")) == 0) {
                    bool userSpaceHDCPTxAuth= false;
                    userSpaceHDCPTxAuth = client->userSpaceHDCPTxAuth();
                    if (userSpaceHDCPTxAuth)
                        printf("userSpaceHDCPTxAuth\n");
                    else
                        printf("not userSpaceHDCPTxAuth\n");
                } else {
                    printf("raw cmd %s is not supported now\n", optarg);
                }
                break;
            case 'F':
               {
                   if (optarg == NULL)
                       break;

                   float framerate = strtod(optarg, NULL);
                   int ret = client->setFrameRate(framerate);
                   printf("setFrameRate  %.2f ret:%d\n",framerate, ret);
               }
               break;
            case 'p':
                 if (optarg == NULL)
                     break;
                  {
                      bool isDisable = false;
                      if (0 == memcmp("true", optarg, sizeof("true"))) {
                            isDisable =true;
                      }
                      client->disableSidebandStream(isDisable);
                      printf("set disableSidebandStream to %s \n", isDisable?"true":"false");
                  }
                break;
            case 'w':
                 if (optarg == NULL)
                     break;
                  {
                      bool mode = false;
                      if (0 == memcmp("true", optarg, sizeof("true"))) {
                            mode =true;
                      }
                      client->setWhiteBoardMode(mode);
                      printf("set white board to %s \n", mode ? "true":"false");
                  }
                break;
            case 'f':
                  {
                      printf("set the white board position (%s %s)\n", optarg, argv[optind]);
                      client->setWBDisplayFrame(strtol(optarg, NULL, 10), strtol(argv[optind], NULL, 10));
                  }
                break;
            case 'h':
                 if (optarg == NULL)
                     break;
                  {
                      bool mode = false;
                      if (0 == memcmp("true", optarg, sizeof("true"))) {
                            mode =true;
                      }
                      client->hideVideoLayer(mode);
                      printf("hide video layer mode is %s \n", mode ? "true":"false");
                  }
                break;
            case 'P':
                 if (optarg == NULL)
                     break;
                  {
                      bool mode = false;
                      if (0 == memcmp("true", optarg, sizeof("true"))) {
                            mode =true;
                      }
                      client->enableSyncProtection(mode);
                      printf("the sync Protection switch to %s \n", mode ? "true":"false");
                  }
                break;
            case 'R':
                 if (optarg == NULL)
                     break;
                  {
                      client->setHdrConversionStrategy(strtol(optarg, NULL, 10),strtol(argv[optind], NULL, 10));
                      printf("setHdrConversionStrategy %s %s \n", optarg, argv[optind]);
                  }
                break;
            case 't':
                if (strspn(optarg, "0123456789") != strlen(optarg))
                {
                    print_usage(argv[0]);
                    break;
                }
                type = static_cast<DisplayAdapter::ConnectorType>(strtol(optarg, NULL, 10));
                // ref DisplayAdapter::ConnectorType
                printf("Connector type changed to %d\n", type);
                break;
            case 'm':
                client->setPerferredMode(optarg, type);
                printf("Set perferred mode to %s\n", optarg);
                break;
            case 'k':
                 if (optarg == NULL)
                     break;
                  {
                      printf("set the keystone params (%s)\n", optarg);
                      client->setKeystoneCorrection(optarg);
                  }
                break;
            case 'V':
                 if (optarg == NULL)
                     break;
                  {
                      printf("set the reverse type to  (%d)\n", (int)strtol(optarg, NULL, 10));
                      client->setReverseMode(strtol(optarg, NULL, 10));
                  }
                break;
            case 'v':
                 if (optarg == NULL)
                     break;
                  {
                      bool isDisable = false;
                      if (0 == memcmp("true", optarg, sizeof("true"))) {
                            isDisable =true;
                      }
                      client->disableQms(isDisable);
                      printf("set disable qms to %s \n", isDisable?"true":"false");
                  }
                break;
            default:
                print_usage(argv[0]);
        }
    };

    DEBUG_INFO("Exit client");
    return 0;
}
