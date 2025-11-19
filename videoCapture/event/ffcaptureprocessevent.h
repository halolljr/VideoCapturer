#ifndef FFCAPTUREPROCESSEVENT_H
#define FFCAPTUREPROCESSEVENT_H

#include"ffevent.h"

class FFCaptureProcessEvent : public FFEvent
{
public:
    FFCaptureProcessEvent(FFCaptureContext* captureCtx,int64_t millseconds_);

    virtual void work()override;

private:
    int64_t millseconds;
};

#endif // FFCAPTUREPROCESSEVENT_H
