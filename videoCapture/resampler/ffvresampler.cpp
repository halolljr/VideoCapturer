#include "ffvresampler.h"
#include"decoder/ffvdecoder.h"

FFVResampler::FFVResampler()
{

}

FFVResampler::~FFVResampler()
{
    if(swsCtx){
        sws_freeContext(swsCtx);
    }
    if(srcPars){
        delete srcPars;
        srcPars = nullptr;
    }
    if(dstPars){
        delete dstPars;
        srcPars = nullptr;
    }
    if(vBuffer){
        av_freep(&vBuffer);
        vBuffer = nullptr;
    }

}

void FFVResampler::init(FFVideoPars *srcPars_, FFVideoPars *dstPars_)
{
    srcPars = new FFVideoPars();
    memcpy(srcPars,srcPars_,sizeof(FFVideoPars));

    dstPars = new FFVideoPars();
    memcpy(dstPars,dstPars_,sizeof(FFVideoPars));

    std::cout<<"==========src video format==========" <<std::endl;
    std::cout<<"width:" << srcPars->width<< std::endl;
    std::cout<<"height:" << srcPars->height<< std::endl;
    std::cout<<"framerate:" << srcPars->frameRate.num << "/" << srcPars->frameRate.den << std::endl;
    std::cout<<"pixFmt:" << av_get_pix_fmt_name(srcPars->pixFmtEnum) << std::endl;

    std::cout<<"==========dst video format==========" <<std::endl;
    std::cout<<"width:" << dstPars->width<< std::endl;
    std::cout<<"height:" << dstPars->height<< std::endl;
    std::cout<<"framerate:" << dstPars->frameRate.num << "/" << dstPars->frameRate.den << std::endl;
    std::cout<<"pixFmt:" << av_get_pix_fmt_name(dstPars->pixFmtEnum) << std::endl;


    initSws();
}

void FFVResampler::resample(AVFrame *srcFrame, AVFrame **dstFrame)
{
    *dstFrame = allocFrame(dstPars,srcFrame);
    if(dstFrame == nullptr){
        return;
    }

    sws_scale(swsCtx,
              srcFrame->data,srcFrame->linesize,0,srcFrame->height,
              (*dstFrame)->data,(*dstFrame)->linesize);
}

AVFrame* FFVResampler::allocFrame(FFVideoPars* vPars,AVFrame* srcFrame)
{
    AVFrame* frame = av_frame_alloc();

    int bufSize = av_image_get_buffer_size(vPars->pixFmtEnum,vPars->width,vPars->height,1);
    if(bufSize > maxbufSize){
        maxbufSize = bufSize;
        if(vBuffer){
            av_freep(&vBuffer);
        }
        vBuffer = static_cast<uint8_t*>(av_mallocz(bufSize));
        if(!bufSize){
            av_frame_free(&frame);
            std::cerr << "malloc vBuffer error!" << std::endl;
            return nullptr;
        }

    }

    int ret = av_image_fill_arrays(frame->data,frame->linesize,vBuffer,
                                   vPars->pixFmtEnum,vPars->width,vPars->height,1);

    if(ret < 0){
        printError(ret);
        av_frame_free(&frame);
        return nullptr;
    }

    frame->width = vPars->width;
    frame->height = vPars->height;
    frame->format = vPars->pixFmtEnum;
    frame->pts = srcFrame->pts;

    return frame;
}

void FFVResampler::initSws()
{
    swsCtx = sws_getContext(srcPars->width,srcPars->height,srcPars->pixFmtEnum,
                            dstPars->width,dstPars->height,dstPars->pixFmtEnum
                            ,SWS_FAST_BILINEAR,nullptr,nullptr,nullptr);

    if(!swsCtx){
        std::cerr << "sws_getContext error!" << std::endl;
        return;
    }

}

void FFVResampler::printError(int ret)
{
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE];
    int res = av_strerror(ret,errorBuffer,sizeof errorBuffer);
    if(res < 0){
        std::cerr << "Unknow Error!" << std::endl;
    }
    else{
        std::cerr << "Error:" << errorBuffer << std::endl;
    }
}
