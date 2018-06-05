// Copyright 2016 Boris Kogan (boris@thekogans.net)
//
// This file is part of libthekogans_crypto.
//
// libthekogans_crypto is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// libthekogans_crypto is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with libthekogans_crypto. If not, see <http://www.gnu.org/licenses/>.

#if defined (THEKOGANS_CRYPTO_TESTING)
    #include <sstream>
#endif // defined (THEKOGANS_CRYPTO_TESTING)
#include <openssl/evp.h>
#include "thekogans/util/Types.h"
#include "thekogans/util/SecureAllocator.h"
#include "thekogans/util/Exception.h"
#if defined (THEKOGANS_CRYPTO_TESTING)
    #include "thekogans/util/StringUtils.h"
    #include "thekogans/util/XMLUtils.h"
#endif // defined (THEKOGANS_CRYPTO_TESTING)
#include "thekogans/crypto/OpenSSLInit.h"
#include "thekogans/crypto/OpenSSLException.h"
#include "thekogans/crypto/AsymmetricKey.h"

namespace thekogans {
    namespace crypto {

        #if !defined (THEKOGANS_CRYPTO_MIN_ASYMMETRIC_KEYS_IN_PAGE)
            #define THEKOGANS_CRYPTO_MIN_ASYMMETRIC_KEYS_IN_PAGE 16
        #endif // !defined (THEKOGANS_CRYPTO_MIN_ASYMMETRIC_KEYS_IN_PAGE)

        THEKOGANS_CRYPTO_IMPLEMENT_SERIALIZABLE (
            AsymmetricKey,
            1,
            THEKOGANS_CRYPTO_MIN_ASYMMETRIC_KEYS_IN_PAGE)

        AsymmetricKey::AsymmetricKey (
                EVP_PKEYPtr key_,
                bool isPrivate_,
                const ID &id,
                const std::string &name,
                const std::string &description) :
                Serializable (id, name, description),
                key (std::move (key_)),
                isPrivate (isPrivate_) {
            if (key.get () != 0) {
                util::i32 type = GetType ();
                if (type != EVP_PKEY_DH &&
                        type != EVP_PKEY_DSA &&
                        type != EVP_PKEY_EC &&
                        type != EVP_PKEY_RSA &&
                        type != EVP_PKEY_HMAC &&
                        type != EVP_PKEY_CMAC) {
                    THEKOGANS_UTIL_THROW_STRING_EXCEPTION (
                        "Invalid parameters type %d.", type);
                }
            }
            else {
                THEKOGANS_UTIL_THROW_ERROR_CODE_EXCEPTION (
                    THEKOGANS_UTIL_OS_ERROR_CODE_EINVAL);
            }
        }

        AsymmetricKey::Ptr AsymmetricKey::LoadPrivateKeyFromFile (
                const std::string &path,
                pem_password_cb *passwordCallback,
                void *userData,
                const ID &id,
                const std::string &name,
                const std::string &description) {
            BIOPtr bio (BIO_new_file (path.c_str (), "r"));
            if (bio.get () != 0) {
                return Ptr (
                    new AsymmetricKey (
                        EVP_PKEYPtr (PEM_read_bio_PrivateKey (bio.get (), 0, passwordCallback, userData)),
                        true,
                        id,
                        name,
                        description));
            }
            else {
                THEKOGANS_CRYPTO_THROW_OPENSSL_EXCEPTION;
            }
        }

        AsymmetricKey::Ptr AsymmetricKey::LoadPublicKeyFromFile (
                const std::string &path,
                pem_password_cb *passwordCallback,
                void *userData,
                const ID &id,
                const std::string &name,
                const std::string &description) {
            BIOPtr bio (BIO_new_file (path.c_str (), "r"));
            if (bio.get () != 0) {
                return Ptr (
                    new AsymmetricKey (
                        EVP_PKEYPtr (PEM_read_bio_PUBKEY (bio.get (), 0, passwordCallback, userData)),
                        false,
                        id,
                        name,
                        description));
            }
            else {
                THEKOGANS_CRYPTO_THROW_OPENSSL_EXCEPTION;
            }
        }

        AsymmetricKey::Ptr AsymmetricKey::LoadPublicKeyFromCertificate (
                const std::string &path,
                pem_password_cb *passwordCallback,
                void *userData,
                const ID &id,
                const std::string &name,
                const std::string &description) {
            BIOPtr bio (BIO_new_file (path.c_str (), "r"));
            if (bio.get () != 0) {
                X509Ptr certificate (PEM_read_bio_X509 (bio.get (), 0, passwordCallback, userData));
                if (certificate.get () != 0) {
                    return Ptr (
                        new AsymmetricKey (
                            EVP_PKEYPtr (X509_get_pubkey (certificate.get ())),
                            false,
                            id,
                            name,
                            description));
                }
                else {
                    THEKOGANS_CRYPTO_THROW_OPENSSL_EXCEPTION;
                }
            }
            else {
                THEKOGANS_CRYPTO_THROW_OPENSSL_EXCEPTION;
            }
        }

        void AsymmetricKey::Save (
                const std::string &path,
                const EVP_CIPHER *cipher,
                const void *symmetricKey,
                std::size_t symmetricKeyLength,
                pem_password_cb *passwordCallback,
                void *userData) {
            BIOPtr bio (BIO_new_file (path.c_str (), "w+"));
            if (bio.get () == 0 || (isPrivate ?
                    PEM_write_bio_PrivateKey (
                        bio.get (),
                        key.get (),
                        cipher,
                        (unsigned char *)symmetricKey,
                        (int)symmetricKeyLength,
                        passwordCallback,
                        userData) :
                    PEM_write_bio_PUBKEY (bio.get (), key.get ())) == 0) {
                THEKOGANS_CRYPTO_THROW_OPENSSL_EXCEPTION;
            }
        }

        AsymmetricKey::Ptr AsymmetricKey::GetPublicKey (
                const ID &id,
                const std::string &name,
                const std::string &description) const {
            BIOPtr bio (BIO_new (BIO_s_mem ()));
            if (PEM_write_bio_PUBKEY (bio.get (), key.get ()) == 1) {
                AsymmetricKey::Ptr publicKey (
                    new AsymmetricKey (
                        EVP_PKEYPtr (PEM_read_bio_PUBKEY (bio.get (), 0, 0, 0)),
                        false,
                        id,
                        name,
                        description));
                if (publicKey->Get () != 0) {
                    return publicKey;
                }
                else {
                    THEKOGANS_CRYPTO_THROW_OPENSSL_EXCEPTION;
                }
            }
            else {
                THEKOGANS_CRYPTO_THROW_OPENSSL_EXCEPTION;
            }
        }

        std::size_t AsymmetricKey::Size () const {
            BIOPtr bio (BIO_new (BIO_s_mem ()));
            if (bio.get () != 0 && (isPrivate ?
                    PEM_write_bio_PrivateKey (bio.get (), key.get (), 0, 0, 0, 0, 0) :
                    PEM_write_bio_PUBKEY (bio.get (), key.get ())) == 1) {
                return
                    Serializable::Size () +
                    util::BOOL_SIZE + // isPrivate
                    util::I32_SIZE + // type
                    util::I32_SIZE + // keyLength
                    BIO_ctrl_pending (bio.get ()); // key
            }
            else {
                THEKOGANS_CRYPTO_THROW_OPENSSL_EXCEPTION;
            }
        }

        void AsymmetricKey::Read (
                const Header &header,
                util::Serializer &serializer) {
            Serializable::Read (header, serializer);
            util::i32 type;
            util::i32 keyLength;
            serializer >> isPrivate >> type >> keyLength;
            util::SecureVector<util::ui8> keyBuffer (keyLength);
            serializer.Read (&keyBuffer[0], keyLength);
            BIOPtr bio (BIO_new (BIO_s_mem ()));
            if (bio.get () != 0) {
                if (BIO_write (bio.get (), &keyBuffer[0], keyLength) == keyLength) {
                    key.reset (isPrivate ?
                        PEM_read_bio_PrivateKey (bio.get (), 0, 0, 0) :
                        PEM_read_bio_PUBKEY (bio.get (), 0, 0, 0));
                }
                else {
                    THEKOGANS_CRYPTO_THROW_OPENSSL_EXCEPTION;
                }
            }
            else {
                THEKOGANS_CRYPTO_THROW_OPENSSL_EXCEPTION;
            }
        }

        namespace {
            void WriteKey (
                    bool isPrivate,
                    EVP_PKEY &key,
                    util::SecureVector<util::ui8> &keyBuffer) {
                BIOPtr bio (BIO_new (BIO_s_mem ()));
                if (bio.get () != 0 && (isPrivate ?
                        PEM_write_bio_PrivateKey (bio.get (), &key, 0, 0, 0, 0, 0) :
                        PEM_write_bio_PUBKEY (bio.get (), &key)) == 1) {
                    util::ui8 *ptr = 0;
                    keyBuffer.resize (BIO_get_mem_data (bio.get (), &ptr));
                    if (!keyBuffer.empty () && ptr != 0) {
                        memcpy (&keyBuffer[0], ptr, keyBuffer.size ());
                    }
                }
                else {
                    THEKOGANS_CRYPTO_THROW_OPENSSL_EXCEPTION;
                }
            }
        }

        void AsymmetricKey::Write (util::Serializer &serializer) const {
            Serializable::Write (serializer);
            util::SecureVector<util::ui8> keyBuffer;
            WriteKey (isPrivate, *key, keyBuffer);
            serializer <<
                isPrivate <<
                GetType () <<
                (util::i32)keyBuffer.size ();
            serializer.Write (&keyBuffer[0], (util::ui32)keyBuffer.size ());
        }

    #if defined (THEKOGANS_CRYPTO_TESTING)
        const char * const AsymmetricKey::ATTR_PRIVATE = "Private";
        const char * const AsymmetricKey::ATTR_KEY_TYPE = "KeyType";

        std::string AsymmetricKey::ToString (
                util::ui32 indentationLevel,
                const char *tagName) const {
            util::SecureVector<util::ui8> keyBuffer;
            WriteKey (isPrivate, *key, keyBuffer);
            std::stringstream stream;
            util::Attributes attributes;
            attributes.push_back (util::Attribute (ATTR_TYPE, Type ()));
            attributes.push_back (util::Attribute (ATTR_ID, id.ToString ()));
            attributes.push_back (util::Attribute (ATTR_NAME, name));
            attributes.push_back (util::Attribute (ATTR_DESCRIPTION, description));
            attributes.push_back (util::Attribute (ATTR_PRIVATE, util::boolTostring (isPrivate)));
            attributes.push_back (util::Attribute (ATTR_KEY_TYPE, EVP_PKEYtypeTostring (GetType ())));
            stream <<
                util::OpenTag (indentationLevel, tagName, attributes, false, true) <<
                std::string (keyBuffer.begin (), keyBuffer.end ()) << std::endl <<
                util::CloseTag (indentationLevel, tagName);
            return stream.str ();
        }
    #endif // defined (THEKOGANS_CRYPTO_TESTING)

    } // namespace crypto
} // namespace thekogans
