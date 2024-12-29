#include "Util.hpp"
#include <optional>

void write_data(uintptr_t address, uint8_t* values, size_t byte_count)
{
    DWORD old_protection;
    void* addr = reinterpret_cast<void*>(address);

    // Change memory protection to allow writing
    if (VirtualProtect(addr, byte_count, PAGE_EXECUTE_READWRITE, &old_protection)) {
        std::memcpy(addr, values, byte_count);
        // Restore the original protection
        VirtualProtect(addr, byte_count, old_protection, &old_protection);
    } else {
        dbg("Failed to change memory protection.");
    }
}

constexpr std::optional<std::string> d3drs_to_string(uint32_t v)
{
    switch (v) {
        case 7: return "D3DRS_ZENABLE";
        case 8: return "D3DRS_FILLMODE";
        case 9: return "D3DRS_SHADEMODE";
        case 14: return "D3DRS_ZWRITEENABLE";
        case 15: return "D3DRS_ALPHATESTENABLE";
        case 16: return "D3DRS_LASTPIXEL";
        case 19: return "D3DRS_SRCBLEND";
        case 20: return "D3DRS_DESTBLEND";
        case 22: return "D3DRS_CULLMODE";
        case 23: return "D3DRS_ZFUNC";
        case 24: return "D3DRS_ALPHAREF";
        case 25: return "D3DRS_ALPHAFUNC";
        case 26: return "D3DRS_DITHERENABLE";
        case 27: return "D3DRS_ALPHABLENDENABLE";
        case 28: return "D3DRS_FOGENABLE";
        case 29: return "D3DRS_SPECULARENABLE";
        case 34: return "D3DRS_FOGCOLOR";
        case 35: return "D3DRS_FOGTABLEMODE";
        case 36: return "D3DRS_FOGSTART";
        case 37: return "D3DRS_FOGEND";
        case 38: return "D3DRS_FOGDENSITY";
        case 48: return "D3DRS_RANGEFOGENABLE";
        case 52: return "D3DRS_STENCILENABLE";
        case 53: return "D3DRS_STENCILFAIL";
        case 54: return "D3DRS_STENCILZFAIL";
        case 55: return "D3DRS_STENCILPASS";
        case 56: return "D3DRS_STENCILFUNC";
        case 57: return "D3DRS_STENCILREF";
        case 58: return "D3DRS_STENCILMASK";
        case 59: return "D3DRS_STENCILWRITEMASK";
        case 60: return "D3DRS_TEXTUREFACTOR";
        case 128: return "D3DRS_WRAP0";
        case 129: return "D3DRS_WRAP1";
        case 130: return "D3DRS_WRAP2";
        case 131: return "D3DRS_WRAP3";
        case 132: return "D3DRS_WRAP4";
        case 133: return "D3DRS_WRAP5";
        case 134: return "D3DRS_WRAP6";
        case 135: return "D3DRS_WRAP7";
        case 136: return "D3DRS_CLIPPING";
        case 137: return "D3DRS_LIGHTING";
        case 139: return "D3DRS_AMBIENT";
        case 140: return "D3DRS_FOGVERTEXMODE";
        case 141: return "D3DRS_COLORVERTEX";
        case 142: return "D3DRS_LOCALVIEWER";
        case 143: return "D3DRS_NORMALIZENORMALS";
        case 145: return "D3DRS_DIFFUSEMATERIALSOURCE";
        case 146: return "D3DRS_SPECULARMATERIALSOURCE";
        case 147: return "D3DRS_AMBIENTMATERIALSOURCE";
        case 148: return "D3DRS_EMISSIVEMATERIALSOURCE";
        case 151: return "D3DRS_VERTEXBLEND";
        case 152: return "D3DRS_CLIPPLANEENABLE";
        case 154: return "D3DRS_POINTSIZE";
        case 155: return "D3DRS_POINTSIZE_MIN";
        case 156: return "D3DRS_POINTSPRITEENABLE";
        case 157: return "D3DRS_POINTSCALEENABLE";
        case 158: return "D3DRS_POINTSCALE_A";
        case 159: return "D3DRS_POINTSCALE_B";
        case 160: return "D3DRS_POINTSCALE_C";
        case 161: return "D3DRS_MULTISAMPLEANTIALIAS";
        case 162: return "D3DRS_MULTISAMPLEMASK";
        case 163: return "D3DRS_PATCHEDGESTYLE";
        case 165: return "D3DRS_DEBUGMONITORTOKEN";
        case 166: return "D3DRS_POINTSIZE_MAX";
        case 167: return "D3DRS_INDEXEDVERTEXBLENDENABLE";
        case 168: return "D3DRS_COLORWRITEENABLE";
        case 170: return "D3DRS_TWEENFACTOR";
        case 171: return "D3DRS_BLENDOP";
        case 172: return "D3DRS_POSITIONDEGREE";
        case 173: return "D3DRS_NORMALDEGREE";
        case 174: return "D3DRS_SCISSORTESTENABLE";
        case 175: return "D3DRS_SLOPESCALEDEPTHBIAS";
        case 176: return "D3DRS_ANTIALIASEDLINEENABLE";
        case 178: return "D3DRS_MINTESSELLATIONLEVEL";
        case 179: return "D3DRS_MAXTESSELLATIONLEVEL";
        case 180: return "D3DRS_ADAPTIVETESS_X";
        case 181: return "D3DRS_ADAPTIVETESS_Y";
        case 182: return "D3DRS_ADAPTIVETESS_Z";
        case 183: return "D3DRS_ADAPTIVETESS_W";
        case 184: return "D3DRS_ENABLEADAPTIVETESSELLATION";
        case 185: return "D3DRS_TWOSIDEDSTENCILMODE";
        case 186: return "D3DRS_CCW_STENCILFAIL";
        case 187: return "D3DRS_CCW_STENCILZFAIL";
        case 188: return "D3DRS_CCW_STENCILPASS";
        case 189: return "D3DRS_CCW_STENCILFUNC";
        case 190: return "D3DRS_COLORWRITEENABLE1";
        case 191: return "D3DRS_COLORWRITEENABLE2";
        case 192: return "D3DRS_COLORWRITEENABLE3";
        case 193: return "D3DRS_BLENDFACTOR";
        case 194: return "D3DRS_SRGBWRITEENABLE";
        case 195: return "D3DRS_DEPTHBIAS";
        case 198: return "D3DRS_WRAP8";
        case 199: return "D3DRS_WRAP9";
        case 200: return "D3DRS_WRAP10";
        case 201: return "D3DRS_WRAP11";
        case 202: return "D3DRS_WRAP12";
        case 203: return "D3DRS_WRAP13";
        case 204: return "D3DRS_WRAP14";
        case 205: return "D3DRS_WRAP15";
        case 206: return "D3DRS_SEPARATEALPHABLENDENABLE";
        case 207: return "D3DRS_SRCBLENDALPHA";
        case 208: return "D3DRS_DESTBLENDALPHA";
        case 209: return "D3DRS_BLENDOPALPHA";
        default: return std::nullopt;
    }
}

void dump_dxstate(IDirect3DDevice9* dev)
{
    for (int i = 7; i < 210; ++i) {
        DWORD v;
        if (auto s = d3drs_to_string(i); s.has_value()) {
            dev->GetRenderState((D3DRENDERSTATETYPE)i, &v);
            dbg(std::format("{}: {}", s.value(), v));
        }
    }
}
