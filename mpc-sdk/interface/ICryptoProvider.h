// interface/ICryptoProvider.h
#pragma once

#include <string>

namespace mpc::crypto {

    // Forward declarations
    class IKeyGenerator;
    class IECDSASigner;
    class IEdDSASigner;

    /**
     * @brief 암호학 Provider 인터페이스
     * 
     * 모든 암호학 라이브러리(Fireblocks, ZenGo, 자체 구현 등)는 
     * 이 인터페이스를 구현해야 합니다.
     * 
     * 이를 통해 암호학 라이브러리를 쉽게 교체할 수 있습니다.
     */
    class ICryptoProvider {
    public:
        virtual ~ICryptoProvider() = default;

        /**
         * @brief 키 생성기 인스턴스 반환
         * @return 키 생성기 (소유권 유지)
         */
        virtual IKeyGenerator* GetKeyGenerator() = 0;

        /**
         * @brief ECDSA 서명기 인스턴스 반환
         * @return ECDSA 서명기 (소유권 유지)
         */
        virtual IECDSASigner* GetECDSASigner() = 0;

        /**
         * @brief EdDSA 서명기 인스턴스 반환
         * @return EdDSA 서명기 (소유권 유지)
         */
        virtual IEdDSASigner* GetEdDSASigner() = 0;

        /**
         * @brief Provider 이름 반환 (예: "Fireblocks", "ZenGo")
         */
        virtual std::string GetProviderName() const = 0;

        /**
         * @brief Provider 버전 반환
         */
        virtual std::string GetVersion() const = 0;
    };
} // namespace mpc::crypto