// src/common/network/tls/src/TlsCertificateLoader.cpp
#include "common/network/tls/include/TlsCertificateLoader.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace mpc_engine::network::tls
{
    TlsCertificateLoader::TlsCertificateLoader(std::shared_ptr<mpc_engine::kms::IKeyManagementService> kms) : kms_(kms)
    {
        if (!kms_) {
            throw std::invalid_argument("KMS cannot be null");
        }

        std::cout << "[TlsCertificateLoader] Initialized for platform: " 
                  << mpc_engine::config::Config::GetString("COORDINATOR_PLATFORM") << std::endl;
    }

    std::string TlsCertificateLoader::LoadCaCertificate() 
    {
        try {
            if (IsLocalPlatform()) {
                std::string ca_path = mpc_engine::config::Config::GetString("TLS_DOCKER_CA");
                std::string ca_cert = ReadPemFile(ca_path);
                
                if (ca_cert.empty()) {
                    std::cerr << "[TlsCertificateLoader] CA certificate file not found: " << ca_path << std::endl;
                    return "";
                }

                std::cout << "[TlsCertificateLoader] Successfully loaded CA certificate from: " << ca_path << std::endl;
                return ca_cert;
            } else {
                std::cerr << "[TlsCertificateLoader] Cloud platform not implemented yet" << std::endl;
                return "";
            }

        } catch (const std::exception& e) {
            std::cerr << "[TlsCertificateLoader] Failed to load CA certificate: " << e.what() << std::endl;
            return "";
        }
    }

    CertificateData TlsCertificateLoader::LoadCoordinatorCertificate() 
    {
        CertificateData cert_data;

        try {
            if (!IsLocalPlatform()) {
                std::cerr << "[TlsCertificateLoader] Cloud platform not implemented yet" << std::endl;
                return cert_data;
            }

            // 인증서 파일 로드
            std::string cert_path = GetCoordinatorCertPath();
            cert_data.certificate_pem = ReadPemFile(cert_path);
            if (cert_data.certificate_pem.empty()) {
                std::cerr << "[TlsCertificateLoader] Coordinator certificate file not found: " << cert_path << std::endl;
                return cert_data;
            }

            // 개인키 파일 로드
            std::string key_path = GetCoordinatorKeyPath();
            cert_data.private_key_pem = ReadPemFile(key_path);
            if (cert_data.private_key_pem.empty()) {
                std::cerr << "[TlsCertificateLoader] Coordinator private key file not found: " << key_path << std::endl;
                return cert_data;
            }

            // CA 체인 로드
            cert_data.ca_chain_pem = LoadCaCertificate();

            std::cout << "[TlsCertificateLoader] Successfully loaded coordinator certificate" << std::endl;
            return cert_data;

        } catch (const std::exception& e) {
            std::cerr << "[TlsCertificateLoader] Failed to load coordinator certificate: " << e.what() << std::endl;
            return CertificateData{};
        }
    }

    CertificateData TlsCertificateLoader::LoadNodeCertificate(int node_index) 
    {
        CertificateData cert_data;

        try {
            if (!IsLocalPlatform()) {
                std::cerr << "[TlsCertificateLoader] Cloud platform not implemented yet" << std::endl;
                return cert_data;
            }

            if (node_index < 0 || node_index > 2) {
                std::cerr << "[TlsCertificateLoader] Invalid node index: " << node_index << std::endl;
                return cert_data;
            }

            // 인증서 파일 로드
            std::string cert_path = GetNodeCertPath(node_index);
            cert_data.certificate_pem = ReadPemFile(cert_path);
            if (cert_data.certificate_pem.empty()) {
                std::cerr << "[TlsCertificateLoader] Node certificate file not found: " << cert_path << std::endl;
                return cert_data;
            }

            // 개인키 파일 로드
            std::string key_path = GetNodeKeyPath(node_index);
            cert_data.private_key_pem = ReadPemFile(key_path);
            if (cert_data.private_key_pem.empty()) {
                std::cerr << "[TlsCertificateLoader] Node private key file not found: " << key_path << std::endl;
                return cert_data;
            }

            // CA 체인 로드
            cert_data.ca_chain_pem = LoadCaCertificate();

            std::cout << "[TlsCertificateLoader] Successfully loaded node certificate for index: " << node_index << std::endl;
            return cert_data;

        } catch (const std::exception& e) {
            std::cerr << "[TlsCertificateLoader] Failed to load node certificate for index " << node_index << ": " << e.what() << std::endl;
            return CertificateData{};
        }
    }

    bool TlsCertificateLoader::IsHealthy() 
    {
        try {
            if (!IsLocalPlatform()) {
                std::cerr << "[TlsCertificateLoader] Only local platform supported currently" << std::endl;
                return false;
            }

            // CA 인증서 존재 확인
            std::string ca_cert = LoadCaCertificate();
            if (ca_cert.empty()) {
                return false;
            }

            return true;

        } catch (const std::exception& e) {
            std::cerr << "[TlsCertificateLoader] Health check failed: " << e.what() << std::endl;
            return false;
        }
    }

    void TlsCertificateLoader::PrintStatus() 
    {
        std::cout << "\n=== TLS Certificate Loader Status ===" << std::endl;
        std::cout << "Platform: " << mpc_engine::config::Config::GetString("COORDINATOR_PLATFORM") << std::endl;
        std::cout << "Healthy: " << (IsHealthy() ? "Yes" : "No") << std::endl;

        if (IsLocalPlatform()) {
            // CA 인증서 확인
            std::cout << "\nCA Certificate: ";
            std::string ca_path = mpc_engine::config::Config::GetString("TLS_DOCKER_CA");
            if (!ReadPemFile(ca_path).empty()) {
                std::cout << "Available (" << ca_path << ")" << std::endl;
            } else {
                std::cout << "Not Found (" << ca_path << ")" << std::endl;
            }

            // Coordinator 인증서 확인
            std::cout << "\nCoordinator Certificate: ";
            std::string coord_cert_path = GetCoordinatorCertPath();
            std::string coord_key_path = GetCoordinatorKeyPath();
            bool coord_cert_exists = !ReadPemFile(coord_cert_path).empty();
            bool coord_key_exists = !ReadPemFile(coord_key_path).empty();
            
            if (coord_cert_exists && coord_key_exists) {
                std::cout << "Available" << std::endl;
            } else {
                std::cout << "Not Complete (cert: " << (coord_cert_exists ? "OK" : "Missing") 
                          << ", key: " << (coord_key_exists ? "OK" : "Missing") << ")" << std::endl;
            }

            // Node 인증서들 확인
            std::cout << "\nNode Certificates:" << std::endl;
            for (int i = 0; i < 3; i++) {
                std::cout << "  node" << (i + 1) << " (index " << i << "): ";
                
                std::string node_cert_path = GetNodeCertPath(i);
                std::string node_key_path = GetNodeKeyPath(i);
                bool node_cert_exists = !ReadPemFile(node_cert_path).empty();
                bool node_key_exists = !ReadPemFile(node_key_path).empty();
                
                if (node_cert_exists && node_key_exists) {
                    std::cout << "Available" << std::endl;
                } else {
                    std::cout << "Not Complete (cert: " << (node_cert_exists ? "OK" : "Missing") 
                              << ", key: " << (node_key_exists ? "OK" : "Missing") << ")" << std::endl;
                }
            }
        }
        std::cout << std::endl;
    }

    // Private 메서드들

    bool TlsCertificateLoader::IsLocalPlatform() 
    {
        std::string platform = mpc_engine::config::Config::GetString("COORDINATOR_PLATFORM");
        return platform == "LOCAL";
    }

    std::string TlsCertificateLoader::ReadPemFile(const std::string& file_path) 
    {
        try {
            if (!std::filesystem::exists(file_path)) {
                return "";
            }

            std::ifstream file(file_path);
            if (!file.is_open()) {
                return "";
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();

        } catch (const std::exception&) {
            return "";
        }
    }

    std::string TlsCertificateLoader::GetCoordinatorCertPath() 
    {
        return mpc_engine::config::Config::GetString("TLS_DOCKER_COORDINATOR");
    }

    std::string TlsCertificateLoader::GetCoordinatorKeyPath() 
    {
        return mpc_engine::config::Config::GetString("TLS_KMS_COORDINATOR_KEY_ID");
    }

    std::string TlsCertificateLoader::GetNodeCertPath(int node_index) 
    {
        auto nodes = mpc_engine::config::Config::GetStringArray("TLS_DOCKER_NODES");
        if (node_index >= 0 && node_index < nodes.size()) {
            return nodes[node_index];
        }
        return "";
    }

    std::string TlsCertificateLoader::GetNodeKeyPath(int node_index) 
    {
        auto keys = mpc_engine::config::Config::GetStringArray("TLS_KMS_NODES_KEY_IDS");
        if (node_index >= 0 && node_index < keys.size()) {
            return keys[node_index];
        }
        return "";
    }

} // namespace mpc_engine::network::tls