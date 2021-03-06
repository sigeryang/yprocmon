/**
 * Copyright (c) 2021 sigeryeung
 *
 * @file yhttp.h
 * @author Siger Yang (sigeryeung@gmail.com)
 * @date 2021-09-10
 *
 * @brief HTTP 服务器
 */

#pragma once

#include "debug.h"
#include "yprocmon.h"
#include <fstream>
#include <httplib.h>
#include <mutex>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

httplib::Server svr;

void start_http_server()
{
    svr.set_pre_routing_handler([](const auto &req, auto &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        return httplib::Server::HandlerResponse::Unhandled;
    });
    svr.set_logger([](const auto &req, const auto &res) {
        console_print("[HTTP] %s %s %s HTTP/1.1 %d\n", req.method.c_str(),
                      req.path.c_str(),
                      req.get_header_value("User-Agent").c_str(), res.status);
    });
    auto ret = svr.set_mount_point("/", "./www");
    svr.Get("/api/ping", [](const httplib::Request &, httplib::Response &res) {
        res.set_content("{\"ping\": true}", "application/json");
    });
    svr.Get("/api/status",
            [](const httplib::Request &, httplib::Response &res) {
                // state.run.
            });
    svr.Get("/api/instances",
            [](const httplib::Request &, httplib::Response &res) {
                rapidjson::StringBuffer s;
                rapidjson::Writer<rapidjson::StringBuffer> writer(s);
                writer.StartArray();
                std::lock_guard<std::mutex> guard(state.state_mutex);
                for (auto const &e : state.instances)
                {
                    e.second.Serialize(writer);
                }
                writer.EndArray();
                res.set_content(s.GetString(), "application/json");
            });
    svr.Get("/api/operations", [&](const httplib::Request &req,
                                   httplib::Response &res) {
        // std::lock_guard<std::mutex> guard(state.operations_mutex);
        // rapidjson::StringBuffer s;
        // rapidjson::Writer<rapidjson::StringBuffer> writer(s);
        // writer.StartArray();
        // if (req.has_param("after"))
        // {
        //     time_t after = atoll(req.get_param_value("after").c_str());
        //     console_print("[HTTP] Filtering operations after timestamp %lld.\n",
        //                   after);
        //     auto start = std::upper_bound(state.operations.begin(),
        //                                   state.operations.end(), after);
        //     for (auto e = start; e != state.operations.end(); e++)
        //     {
        //         e->Serialize(writer);
        //     }
        // }
        // else
        // {
        //     for (auto const &e : state.operations)
        //     {
        //         e.Serialize(writer);
        //     }
        // }
        // writer.EndArray();
        // res.set_content(s.GetString(), "application/json");
        res.set_content("[]", "application/json");
    });
    // svr.Get("/api/info",
    //          [&](const auto &req, auto &res)
    //          {
    //             //  std::string name, command;
    //             //  if (req.has_param("name"))
    //          });
    svr.Get("/api/files", [](const httplib::Request &, httplib::Response &res) {
        rapidjson::StringBuffer s;
        rapidjson::Writer<rapidjson::StringBuffer> writer(s);
        writer.StartArray();
        for (auto const &f : get_files())
        {
            f.Serialize(writer);
        }
        writer.EndArray();
        res.set_content(s.GetString(), "application/json");
    });
    svr.Post("/api/upload", [&](const auto &req, auto &res) {
        if (req.has_file("file"))
        {
            const auto &file = req.get_file_value("file");
            // std::ofstream out()
            std::string path("samples/");
            path += file.filename;
            std::ofstream out(path, std::ios::binary);
            out << file.content;
            out.close();
        }
        else
        {
            res.set_content("{\"error\": true}", "application/json");
        }
    });
    svr.Post(
        "/api/run", [](const httplib::Request &req, httplib::Response &res) {
            std::string name, command;
            if (req.has_param("name"))
            {
                name = req.get_param_value("name");
                command = req.get_param_value("name");
                if (req.has_param("command"))
                {
                    command = req.get_param_value("command");
                }
                detour_startup s;
                instance_entry e;
                s.name = name;
                s.command = command;
                bool success = detour_program(s, e);
                if (success)
                {
                    rapidjson::StringBuffer s;
                    rapidjson::Writer<rapidjson::StringBuffer> writer(s);
                    e.Serialize(writer);
                    res.set_content(s.GetString(), "application/json");
                    std::lock_guard<std::mutex> guard(state.state_mutex);
                    state.instances[e.pi.dwProcessId] = (std::move(e));
                }
                else
                {
                    res.status = 500;
                    res.set_content("{\"run\": false}", "application/json");
                }
            }
            else
            {
                res.status = 500;
                res.set_content("{\"run\": false}", "application/json");
            }
        });
    svr.Post("/api/stop",
             [](const httplib::Request &req, httplib::Response &res) {});
    svr.listen("0.0.0.0", state.port);
}