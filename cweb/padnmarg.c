#include "padnmarg.h"

cweb_padding cweb_pad(float val){
    return (cweb_padding){val, val, val, val};
}

cweb_margin cweb_marg(float val){
    return (cweb_margin){val, val, val, val};
}


cweb_padding cweb_pad_x(float val){
    return (cweb_padding){
        .left = val,
        .right = val,
    };
}

cweb_padding cweb_pad_y(float val){
    return (cweb_padding){
        .top = val,
        .bottom = val,
    };
}


cweb_margin cweb_marg_x(float val){
    return (cweb_margin){
        .left = val,
        .right = val,
    };
}

cweb_margin cweb_marg_y(float val){
    return (cweb_margin){
        .top = val,
        .bottom = val,
    };
}

bool cweb_pad_nz(cweb_padding pad) {
    return pad.bottom > 0 || pad.top > 0 || pad.left > 0 || pad.right > 0;
}

bool cweb_marg_nz(cweb_margin marg) {
    return marg.bottom > 0 || marg.top > 0 || marg.left > 0 || marg.right > 0;
}
