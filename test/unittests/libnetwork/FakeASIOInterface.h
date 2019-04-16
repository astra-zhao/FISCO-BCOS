/**
 * @CopyRight:
 * FISCO-BCOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FISCO-BCOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FISCO-BCOS.  If not, see <http://www.gnu.org/licenses/>
 * (c) 2016-2018 fisco-dev contributors.
 *
 * @brief: inteface for boost::asio(for unittest)
 *
 * @file AsioInterface.h
 * @author: yujiechen
 * @date 2018-09-13
 */
#pragma once
#include "FakeSocket.h"
#include <libnetwork/ASIOInterface.h>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace dev
{
namespace network
{
class FakeASIOInterface : public ASIOInterface
{
public:
    /// CompletionHandler
    typedef boost::function<void()> Base_Handler;
    /// accept handler
    typedef boost::function<void(const boost::system::error_code)> Handler_Type;
    /// write handler
    typedef boost::function<void(const boost::system::error_code, std::size_t)> ReadWriteHandler;
    typedef boost::function<bool(bool, boost::asio::ssl::verify_context&)> VerifyCallback;

    ~FakeASIOInterface() = default;
    void setType(int type) override { m_type = type; }

    std::shared_ptr<ba::io_service> ioService() override { return m_ioService; }
    void setIOService(std::shared_ptr<ba::io_service> ioService) override
    {
        m_ioService = ioService;
    }

    std::shared_ptr<ba::ssl::context> sslContext() override { return m_sslContext; }
    void setSSLContext(std::shared_ptr<ba::ssl::context> sslContext) override
    {
        m_sslContext = sslContext;
    }

    std::shared_ptr<boost::asio::deadline_timer> newTimer(uint32_t timeout) override
    {
        return std::make_shared<boost::asio::deadline_timer>(
            *m_ioService, boost::posix_time::milliseconds(timeout));
    }

    std::shared_ptr<SocketFace> newSocket(NodeIPEndpoint nodeIPEndpoint = NodeIPEndpoint()) override
    {
        return std::make_shared<FakeSocket>(*m_ioService, *m_sslContext, nodeIPEndpoint);
    }

    std::shared_ptr<bi::tcp::acceptor> acceptor() override { return m_acceptor; }

    void init(std::string listenHost, uint16_t listenPort) override
    {
        m_strand = std::make_shared<boost::asio::io_service::strand>(*m_ioService);
        m_acceptor = std::make_shared<bi::tcp::acceptor>(*m_ioService);
        m_listenHost = listenHost;
        m_listenPort = listenPort;
    }

    void run() override { std::this_thread::sleep_for(std::chrono::seconds(5)); }

    void stop() override {}

    void reset() override {}

    void asyncAccept(std::shared_ptr<SocketFace> socket, Handler_Type handler,
        boost::system::error_code = boost::system::error_code()) override
    {
        // m_acceptor->async_accept(socket->ref(), m_strand->wrap(handler));
        m_acceptorInfo = std::make_pair(socket, handler);
    }

    void asyncConnect(std::shared_ptr<SocketFace> socket, const bi::tcp::endpoint peer_endpoint,
        Handler_Type handler, boost::system::error_code = boost::system::error_code()) override
    {
        auto fakeSocket = std::dynamic_pointer_cast<FakeSocket>(socket);
        fakeSocket->open();
        fakeSocket->setRemoteEndpoint(peer_endpoint);
        boost::system::error_code ec;
        handler(ec);
    }

    void asyncWrite(std::shared_ptr<SocketFace> socket, boost::asio::mutable_buffers_1 buffers,
        ReadWriteHandler handler) override
    {
        m_ioService->post([socket, buffers, handler]() {
            if (socket->isConnected())
            {
                auto fakeSocket = std::dynamic_pointer_cast<FakeSocket>(socket);
                fakeSocket->write(buffers);
                boost::system::error_code ec;
                handler(ec, buffers.size());
            }
        });
    }

    void asyncRead(
        std::shared_ptr<SocketFace>, boost::asio::mutable_buffers_1, ReadWriteHandler) override
    {}

    void asyncReadSome(std::shared_ptr<SocketFace> socket, boost::asio::mutable_buffers_1 buffers,
        ReadWriteHandler handler) override
    {
        auto fakeSocket = std::dynamic_pointer_cast<FakeSocket>(socket);
        auto size = fakeSocket->read(buffers);
        boost::system::error_code ec;
        handler(ec, size);
    }

    void asyncHandshake(std::shared_ptr<SocketFace> socket,
        ba::ssl::stream_base::handshake_type type, Handler_Type handler) override
    {
        (void)socket;
        (void)type;
        boost::system::error_code ec;
        handler(ec);
    }

    void asyncWait(boost::asio::deadline_timer* m_timer, boost::asio::io_service::strand& m_strand,
        Handler_Type handler, boost::system::error_code = boost::system::error_code()) override
    {
        if (m_timer)
            m_timer->async_wait(m_strand.wrap(handler));
    }

    void setVerifyCallback(
        std::shared_ptr<SocketFace> socket, VerifyCallback callback, bool = true) override
    {
        (void)socket;
        // auto s = m_sslContext->m_sslContext();
        // auto x509 = SSL_CTX_get0_certificate(s);
        // auto store = SSL_CTX_get_cert_store(s);
        auto x509_store_ctx = X509_STORE_CTX_new();
        // auto chain = SSL_get_peer_cert_chain();
        // X509_STORE_CTX_init(x509_store_ctx,store,x509,chain);
        boost::asio::ssl::verify_context verifyContext(x509_store_ctx);
        callback(true, verifyContext);
    }

    void strandPost(Base_Handler handler) override { m_strand->post(handler); }
    // std::shared_ptr<SocketFace> m_socket;
    std::pair<std::shared_ptr<SocketFace>, Handler_Type> m_acceptorInfo;
    std::string m_listenHost;
    uint16_t m_listenPort;

private:
    std::shared_ptr<ba::io_service> m_ioService;
    std::shared_ptr<ba::io_service::strand> m_strand;
    std::shared_ptr<bi::tcp::acceptor> m_acceptor = nullptr;
    std::shared_ptr<ba::ssl::context> m_sslContext;
    int m_type = 0;
};
}  // namespace network
}  // namespace dev
