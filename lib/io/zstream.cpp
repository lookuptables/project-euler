#include "lib/io/zstream.h"

ZException::ZException(int ret) : msg_("zlib: ") {
    switch (ret) {
        case Z_STREAM_ERROR:
            msg_ += "Z_STREAM_ERROR: ";
            break;
        case Z_DATA_ERROR:
            msg_ += "Z_DATA_ERROR: ";
            break;
        case Z_MEM_ERROR:
            msg_ += "Z_MEM_ERROR: ";
            break;
        case Z_VERSION_ERROR:
            msg_ += "Z_VERSION_ERROR: ";
            break;
        case Z_BUF_ERROR:
            msg_ += "Z_BUF_ERROR: ";
            break;
        default:
            msg_ += string("[Unknown: ") + to_string(ret) + "]";
            break;
    }
}

ZException::ZException(string msg) : msg_(move(msg)) {}

const char* ZException::what() const noexcept { return msg_.c_str(); }

ZStreamWrapper::ZStreamWrapper(bool isInput, int level) : isInput_(isInput) {
    zalloc = Z_NULL;
    zfree = Z_NULL;
    opaque = Z_NULL;
    int ret;
    if (isInput_) {
        avail_in = 0;
        next_in = Z_NULL;
        ret = inflateInit2(this, 15 + 32);
    } else {
        ret = deflateInit2(this, level, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
    }
    if (ret != Z_OK) {
        throw ZException(ret);
    }
}

ZStreamWrapper::~ZStreamWrapper() {
    if (isInput_) {
        inflateEnd(this);
    } else {
        deflateEnd(this);
    }
}

ZIStreamBuf::ZIStreamBuf(streambuf* pSBuf, size_t buffSize) : pSBuf_(pSBuf), buffSize_(buffSize) {
    assert(pSBuf);
    inBuff_.resize(buffSize_);
    inBuffStart_ = inBuff_.data();
    inBuffEnd_ = inBuff_.data();
    outBuff_.resize(buffSize_);
    setg(outBuff_.data(), outBuff_.data(), outBuff_.data());
}

streambuf::int_type ZIStreamBuf::underflow() {
    if (gptr() == egptr()) {
        char* outBuffFreeStart = outBuff_.data();
        do {
            if (inBuffStart_ == inBuffEnd_) {
                inBuffStart_ = inBuff_.data();
                auto sz = pSBuf_->sgetn(inBuff_.data(), buffSize_);
                inBuffEnd_ = inBuff_.data() + sz;
                if (inBuffEnd_ == inBuffStart_) {
                    break;
                }
            }
            zStrm_.next_in = reinterpret_cast<Bytef*>(inBuffStart_);
            zStrm_.avail_in = inBuffEnd_ - inBuffStart_;
            zStrm_.next_out = reinterpret_cast<Bytef*>(outBuffFreeStart);
            zStrm_.avail_out = (outBuff_.data() + buffSize_) - outBuffFreeStart;
            auto ret = inflate(&zStrm_, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END) {
                throw ZException(ret);
            }
            inBuffStart_ = reinterpret_cast<char*>(zStrm_.next_in);
            inBuffEnd_ = inBuffStart_ + zStrm_.avail_in;
            outBuffFreeStart = reinterpret_cast<char*>(zStrm_.next_out);
            assert(outBuffFreeStart + zStrm_.avail_out == outBuff_.data() + buffSize_);
        } while (outBuffFreeStart == outBuff_.data());
        setg(outBuff_.data(), outBuff_.data(), outBuffFreeStart);
    }
    return (gptr() == egptr()) ? traits_type::eof() : traits_type::to_int_type(*gptr());
}

ZIStream::ZIStream(shared_ptr<istream> stream) : istream(new ZIStreamBuf(stream->rdbuf())), stream_(stream) {}

ZIStream::~ZIStream() {
    delete rdbuf();
}
