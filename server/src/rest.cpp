// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "httpserver.h"
#include "streams.h"
#include "sync.h"
#include "version.h"
#include "httpserver.h"
#include <boost/algorithm/string.hpp>
#include "rpc/protocol.h"
#include "uint256.h"
#include "utilstrencodings.h"
#include <iostream>
#include "rpc/server.h"
// Allow a max of 15 outpoints to be queried at once.
static const size_t MAX_GETUTXOS_OUTPOINTS = 15;

enum RetFormat {
    RF_UNDEF,
    RF_BINARY,
    RF_HEX,
    RF_JSON,
};

static const struct {
    enum RetFormat rf;
    const char *name;
} rf_names[] = {
    {RF_UNDEF, ""}, {RF_BINARY, "bin"}, {RF_HEX, "hex"}, {RF_JSON, "json"},
};


static bool RESTERR(HTTPRequest *req, enum HTTPStatusCode status,
                    std::string message) {
    req->WriteHeader("Content-Type", "text/plain");
    req->WriteReply(status, message + "\r\n");
    return false;
}

static enum RetFormat ParseDataFormat(std::string &param,
                                      const std::string &strReq) {
    const std::string::size_type pos = strReq.rfind('.');
    if (pos == std::string::npos) {
        param = strReq;
        return rf_names[0].rf;
    }

    param = strReq.substr(0, pos);
    const std::string suff(strReq, pos + 1);

    for (size_t i = 0; i < ARRAYLEN(rf_names); i++) {
        if (suff == rf_names[i].name) {
            return rf_names[i].rf;
        }
    }

    /* If no suffix is found, return original string.  */
    param = strReq;
    return rf_names[0].rf;
}

static std::string AvailableDataFormatsString() {
    std::string formats = "";
    for (size_t i = 0; i < ARRAYLEN(rf_names); i++) {
        if (strlen(rf_names[i].name) > 0) {
            formats.append(".");
            formats.append(rf_names[i].name);
            formats.append(", ");
        }
    }

    if (formats.length() > 0) {
        return formats.substr(0, formats.length() - 2);
    }

    return formats;
}

static bool ParseHashStr(const std::string &strReq, uint256 &v) {
    if (!IsHex(strReq) || (strReq.size() != 64)) {
        return false;
    }

    v.SetHex(strReq);
    return true;
}

static bool CheckWarmup(HTTPRequest *req) {
    std::string statusmessage;
    if (RPCIsInWarmup(&statusmessage)) {
        return RESTERR(req, HTTP_SERVICE_UNAVAILABLE,
                       "Service temporarily unavailable: " + statusmessage);
    }

    return true;
}

static bool rest_headers( HTTPRequest *req,
                         const std::string &strURIPart) {
    if (!CheckWarmup(req)) {
        return false;
    }

    std::string param;
    const RetFormat rf = ParseDataFormat(param, strURIPart);
    std::vector<std::string> path;
    boost::split(path, param, boost::is_any_of("/"));

    if (path.size() != 2) {
        return RESTERR(req, HTTP_BAD_REQUEST, "No header count specified. Use "
                                              "/rest/headers/<count>/"
                                              "<hash>.<ext>.");
    }

    long count = strtol(path[0].c_str(), nullptr, 10);
    if (count < 1 || count > 2000) {
        return RESTERR(req, HTTP_BAD_REQUEST,
                       "Header count out of range: " + path[0]);
    }

    std::string hashStr = path[1];
    uint256 hash;
/*    if (!ParseHashStr(hashStr, hash)) {
        return RESTERR(req, HTTP_BAD_REQUEST, "Invalid hash: " + hashStr);
    }*/
    CDataStream ssHeader(SER_NETWORK, PROTOCOL_VERSION);
    ssHeader << hashStr;

    switch (rf) {
        case RF_BINARY: {
            std::string binaryHeader = ssHeader.str();
            req->WriteHeader("Content-Type", "application/octet-stream");
            req->WriteReply(HTTP_OK, binaryHeader);
            return true;
        }

        case RF_HEX: {
            std::string strHex =
                HexStr(ssHeader.begin(), ssHeader.end()) + "\n";
            req->WriteHeader("Content-Type", "text/plain");
            req->WriteReply(HTTP_OK, strHex);
            return true;
        }
        case RF_JSON: {
            json jsonHeaders;
            jsonHeaders["data"] = "header";
            std::string strJSON = jsonHeaders.dump() + "\n";
            req->WriteHeader("Content-Type", "application/json");
            req->WriteReply(HTTP_OK, strJSON);
            return true;
        }
        default: {
            return RESTERR(req, HTTP_NOT_FOUND,
                           "output format not found (available: .bin, .hex)");
        }
    }

    // not reached
    // continue to process further HTTP reqs on this cxn
    return true;
}


static bool rest_chaininfo(HTTPRequest *req,
                           const std::string &strURIPart) {
    if (!CheckWarmup(req)) {
        return false;
    }

    std::string param;
    const RetFormat rf = ParseDataFormat(param, strURIPart);

    switch (rf) {
        case RF_JSON: {
            JSONRPCRequest jsonRequest;
            jsonRequest.params;
            json chainInfoObject ;
            chainInfoObject["chain"] = "test";
            std::string strJSON = chainInfoObject.dump() + "\n";
            req->WriteHeader("Content-Type", "application/json");
            req->WriteReply(HTTP_OK, strJSON);
            return true;
        }
        default: {
            return RESTERR(req, HTTP_NOT_FOUND,
                           "output format not found (available: json)");
        }
    }

    // not reached
    // continue to process further HTTP reqs on this cxn
    return true;
}



 
static const struct {
    const char *prefix;
    bool (*handler)(HTTPRequest *req,
                    const std::string &strReq);
} uri_prefixes[] = {
    {"/rest/chaininfo", rest_chaininfo},
    {"/rest/headers/", rest_headers},
};

bool StartREST() {
    for (size_t i = 0; i < ARRAYLEN(uri_prefixes); i++) {
        RegisterHTTPHandler(uri_prefixes[i].prefix, false,
                            uri_prefixes[i].handler);
    }

    return true;
}

void InterruptREST() {}

void StopREST() {
    for (size_t i = 0; i < ARRAYLEN(uri_prefixes); i++) {
        UnregisterHTTPHandler(uri_prefixes[i].prefix, false);
    }
}
