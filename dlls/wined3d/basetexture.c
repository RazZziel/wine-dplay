/*
 * IWineD3DBaseTexture Implementation
 *
 * Copyright 2002-2004 Jason Edmeades
 * Copyright 2002-2004 Raphael Junqueira
 * Copyright 2005 Oliver Stieber
 * Copyright 2007-2008 Stefan Dösinger for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"
#include "wined3d_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3d_texture);
#define GLINFO_LOCATION This->resource.wineD3DDevice->adapter->gl_info

HRESULT basetexture_init(IWineD3DBaseTextureImpl *texture, UINT levels, WINED3DRESOURCETYPE resource_type,
        IWineD3DDeviceImpl *device, UINT size, DWORD usage, const struct GlPixelFormatDesc *format_desc,
        WINED3DPOOL pool, IUnknown *parent)
{
    HRESULT hr;

    hr = resource_init((IWineD3DResource *)texture, resource_type, device, size, usage, format_desc, pool, parent);
    if (FAILED(hr))
    {
        WARN("Failed to initialize resource, returning %#x\n", hr);
        return hr;
    }

    texture->baseTexture.levels = levels;
    texture->baseTexture.filterType = (usage & WINED3DUSAGE_AUTOGENMIPMAP) ? WINED3DTEXF_LINEAR : WINED3DTEXF_NONE;
    texture->baseTexture.LOD = 0;
    texture->baseTexture.dirty = TRUE;
    texture->baseTexture.srgbDirty = TRUE;
    texture->baseTexture.is_srgb = FALSE;
    texture->baseTexture.pow2Matrix_identity = TRUE;

    if (texture->resource.format_desc->Flags & WINED3DFMT_FLAG_FILTERING)
    {
        texture->baseTexture.minMipLookup = minMipLookup;
        texture->baseTexture.magLookup = magLookup;
    }
    else
    {
        texture->baseTexture.minMipLookup = minMipLookup_noFilter;
        texture->baseTexture.magLookup = magLookup_noFilter;
    }

    return WINED3D_OK;
}

void basetexture_cleanup(IWineD3DBaseTexture *iface)
{
    basetexture_unload(iface);
    resource_cleanup((IWineD3DResource *)iface);
}

void basetexture_unload(IWineD3DBaseTexture *iface)
{
    IWineD3DTextureImpl *This = (IWineD3DTextureImpl *)iface;
    IWineD3DDeviceImpl *device = This->resource.wineD3DDevice;

    if(This->baseTexture.textureName) {
        ActivateContext(device, NULL, CTXUSAGE_RESOURCELOAD);
        ENTER_GL();
        glDeleteTextures(1, &This->baseTexture.textureName);
        This->baseTexture.textureName = 0;
        LEAVE_GL();
    }

    if(This->baseTexture.srgbTextureName) {
        ActivateContext(device, NULL, CTXUSAGE_RESOURCELOAD);
        ENTER_GL();
        glDeleteTextures(1, &This->baseTexture.srgbTextureName);
        This->baseTexture.srgbTextureName = 0;
        LEAVE_GL();
    }
    This->baseTexture.dirty = TRUE;
    This->baseTexture.srgbDirty = TRUE;
}

DWORD basetexture_set_lod(IWineD3DBaseTexture *iface, DWORD LODNew)
{
    IWineD3DBaseTextureImpl *This = (IWineD3DBaseTextureImpl *)iface;
    DWORD old = This->baseTexture.LOD;

    /* The d3d9:texture test shows that SetLOD is ignored on non-managed
     * textures. The call always returns 0, and GetLOD always returns 0
     */
    if (This->resource.pool != WINED3DPOOL_MANAGED) {
        TRACE("Ignoring SetLOD on %s texture, returning 0\n", debug_d3dpool(This->resource.pool));
        return 0;
    }

    if(LODNew >= This->baseTexture.levels)
        LODNew = This->baseTexture.levels - 1;

    if(This->baseTexture.LOD != LODNew) {
        This->baseTexture.LOD = LODNew;

        This->baseTexture.states[WINED3DTEXSTA_MAXMIPLEVEL] = ~0U;
        This->baseTexture.srgbstates[WINED3DTEXSTA_MAXMIPLEVEL] = ~0U;
        if(This->baseTexture.bindCount) {
            IWineD3DDeviceImpl_MarkStateDirty(This->resource.wineD3DDevice, STATE_SAMPLER(This->baseTexture.sampler));
        }
    }

    TRACE("(%p) : set LOD to %d\n", This, This->baseTexture.LOD);

    return old;
}

DWORD basetexture_get_lod(IWineD3DBaseTexture *iface)
{
    IWineD3DBaseTextureImpl *This = (IWineD3DBaseTextureImpl *)iface;

    TRACE("(%p) : returning %d\n", This, This->baseTexture.LOD);

    return This->baseTexture.LOD;
}

DWORD basetexture_get_level_count(IWineD3DBaseTexture *iface)
{
    IWineD3DBaseTextureImpl *This = (IWineD3DBaseTextureImpl *)iface;
    TRACE("(%p) : returning %d\n", This, This->baseTexture.levels);
    return This->baseTexture.levels;
}

HRESULT basetexture_set_autogen_filter_type(IWineD3DBaseTexture *iface, WINED3DTEXTUREFILTERTYPE FilterType)
{
  IWineD3DBaseTextureImpl *This = (IWineD3DBaseTextureImpl *)iface;
  IWineD3DDeviceImpl *device = This->resource.wineD3DDevice;
  UINT textureDimensions = IWineD3DBaseTexture_GetTextureDimensions(iface);

  if (!(This->resource.usage & WINED3DUSAGE_AUTOGENMIPMAP)) {
      TRACE("(%p) : returning invalid call\n", This);
      return WINED3DERR_INVALIDCALL;
  }
  if(This->baseTexture.filterType != FilterType) {
      /* What about multithreading? Do we want all the context overhead just to set this value?
       * Or should we delay the applying until the texture is used for drawing? For now, apply
       * immediately.
       */
      ActivateContext(device, NULL, CTXUSAGE_RESOURCELOAD);
      ENTER_GL();
      glBindTexture(textureDimensions, This->baseTexture.textureName);
      checkGLcall("glBindTexture");
      switch(FilterType) {
          case WINED3DTEXF_NONE:
          case WINED3DTEXF_POINT:
              glTexParameteri(textureDimensions, GL_GENERATE_MIPMAP_HINT_SGIS, GL_FASTEST);
              checkGLcall("glTexParameteri(textureDimensions, GL_GENERATE_MIPMAP_HINT_SGIS, GL_FASTEST)");

              break;
          case WINED3DTEXF_LINEAR:
              glTexParameteri(textureDimensions, GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);
              checkGLcall("glTexParameteri(textureDimensions, GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST)");

              break;
          default:
              WARN("Unexpected filter type %d, setting to GL_NICEST\n", FilterType);
              glTexParameteri(textureDimensions, GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);
              checkGLcall("glTexParameteri(textureDimensions, GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST)");
      }
      LEAVE_GL();
  }
  This->baseTexture.filterType = FilterType;
  TRACE("(%p) :\n", This);
  return WINED3D_OK;
}

WINED3DTEXTUREFILTERTYPE basetexture_get_autogen_filter_type(IWineD3DBaseTexture *iface)
{
    IWineD3DBaseTextureImpl *This = (IWineD3DBaseTextureImpl *)iface;

    FIXME("(%p) : stub\n", This);

    return This->baseTexture.filterType;
}

void basetexture_generate_mipmaps(IWineD3DBaseTexture *iface)
{
  IWineD3DBaseTextureImpl *This = (IWineD3DBaseTextureImpl *)iface;
  /* TODO: implement filters using GL_SGI_generate_mipmaps http://oss.sgi.com/projects/ogl-sample/registry/SGIS/generate_mipmap.txt */
  FIXME("(%p) : stub\n", This);
  return ;
}

BOOL basetexture_set_dirty(IWineD3DBaseTexture *iface, BOOL dirty)
{
    BOOL old;
    IWineD3DBaseTextureImpl *This = (IWineD3DBaseTextureImpl *)iface;
    old = This->baseTexture.dirty || This->baseTexture.srgbDirty;
    This->baseTexture.dirty = dirty;
    This->baseTexture.srgbDirty = dirty;
    return old;
}

BOOL basetexture_get_dirty(IWineD3DBaseTexture *iface)
{
    IWineD3DBaseTextureImpl *This = (IWineD3DBaseTextureImpl *)iface;
    return This->baseTexture.dirty || This->baseTexture.srgbDirty;
}

/* Context activation is done by the caller. */
HRESULT basetexture_bind(IWineD3DBaseTexture *iface, BOOL srgb, BOOL *set_surface_desc)
{
    IWineD3DBaseTextureImpl *This = (IWineD3DBaseTextureImpl *)iface;
    HRESULT hr = WINED3D_OK;
    UINT textureDimensions;
    BOOL isNewTexture = FALSE;
    GLuint *texture;
    DWORD *states;
    TRACE("(%p) : About to bind texture\n", This);

    This->baseTexture.is_srgb = srgb; /* SRGB mode cache for PreLoad calls outside drawprim */
    if(srgb) {
        texture = &This->baseTexture.srgbTextureName;
        states = This->baseTexture.srgbstates;
    } else {
        texture = &This->baseTexture.textureName;
        states = This->baseTexture.states;
    }

    textureDimensions = IWineD3DBaseTexture_GetTextureDimensions(iface);
    ENTER_GL();
    /* Generate a texture name if we don't already have one */
    if (*texture == 0) {
        *set_surface_desc = TRUE;
        glGenTextures(1, texture);
        checkGLcall("glGenTextures");
        TRACE("Generated texture %d\n", *texture);
        if (This->resource.pool == WINED3DPOOL_DEFAULT) {
            /* Tell opengl to try and keep this texture in video ram (well mostly) */
            GLclampf tmp;
            tmp = 0.9f;
            glPrioritizeTextures(1, texture, &tmp);

        }
        /* Initialise the state of the texture object
        to the openGL defaults, not the directx defaults */
        states[WINED3DTEXSTA_ADDRESSU]      = WINED3DTADDRESS_WRAP;
        states[WINED3DTEXSTA_ADDRESSV]      = WINED3DTADDRESS_WRAP;
        states[WINED3DTEXSTA_ADDRESSW]      = WINED3DTADDRESS_WRAP;
        states[WINED3DTEXSTA_BORDERCOLOR]   = 0;
        states[WINED3DTEXSTA_MAGFILTER]     = WINED3DTEXF_LINEAR;
        states[WINED3DTEXSTA_MINFILTER]     = WINED3DTEXF_POINT; /* GL_NEAREST_MIPMAP_LINEAR */
        states[WINED3DTEXSTA_MIPFILTER]     = WINED3DTEXF_LINEAR; /* GL_NEAREST_MIPMAP_LINEAR */
        states[WINED3DTEXSTA_MAXMIPLEVEL]   = 0;
        states[WINED3DTEXSTA_MAXANISOTROPY] = 1;
        states[WINED3DTEXSTA_SRGBTEXTURE]   = 0;
        states[WINED3DTEXSTA_ELEMENTINDEX]  = 0;
        states[WINED3DTEXSTA_DMAPOFFSET]    = 0;
        states[WINED3DTEXSTA_TSSADDRESSW]   = WINED3DTADDRESS_WRAP;
        IWineD3DBaseTexture_SetDirty(iface, TRUE);
        isNewTexture = TRUE;

        if(This->resource.usage & WINED3DUSAGE_AUTOGENMIPMAP) {
            /* This means double binding the texture at creation, but keeps the code simpler all
             * in all, and the run-time path free from additional checks
             */
            glBindTexture(textureDimensions, *texture);
            checkGLcall("glBindTexture");
            glTexParameteri(textureDimensions, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
            checkGLcall("glTexParameteri(textureDimensions, GL_GENERATE_MIPMAP_SGIS, GL_TRUE)");
        }
    } else {
        *set_surface_desc = FALSE;
    }

    /* Bind the texture */
    if (*texture != 0) {
        glBindTexture(textureDimensions, *texture);
        checkGLcall("glBindTexture");
        if (isNewTexture) {
            /* For a new texture we have to set the textures levels after binding the texture.
             * In theory this is all we should ever have to do, but because ATI's drivers are broken, we
             * also need to set the texture dimensions before the texture is set
             * Beware that texture rectangles do not support mipmapping, but set the maxmiplevel if we're
             * relying on the partial GL_ARB_texture_non_power_of_two emulation with texture rectangles
             * (ie, do not care for cond_np2 here, just look for GL_TEXTURE_RECTANGLE_ARB)
             */
            if(textureDimensions != GL_TEXTURE_RECTANGLE_ARB) {
                TRACE("Setting GL_TEXTURE_MAX_LEVEL to %d\n", This->baseTexture.levels - 1);
                glTexParameteri(textureDimensions, GL_TEXTURE_MAX_LEVEL, This->baseTexture.levels - 1);
                checkGLcall("glTexParameteri(textureDimensions, GL_TEXTURE_MAX_LEVEL, This->baseTexture.levels)");
            }
            if(textureDimensions==GL_TEXTURE_CUBE_MAP_ARB) {
                /* Cubemaps are always set to clamp, regardless of the sampler state. */
                glTexParameteri(textureDimensions, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(textureDimensions, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glTexParameteri(textureDimensions, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            }
        }
    } else { /* this only happened if we've run out of openGL textures */
        WARN("This texture doesn't have an openGL texture assigned to it\n");
        hr =  WINED3DERR_INVALIDCALL;
    }

    LEAVE_GL();
    return hr;
}

/* GL locking is done by the caller */
static inline void apply_wrap(const GLint textureDimensions, const DWORD state, const GLint type,
                              BOOL cond_np2) {
    GLint wrapParm;

    if (state < minLookup[WINELOOKUP_WARPPARAM] || state > maxLookup[WINELOOKUP_WARPPARAM]) {
        FIXME("Unrecognized or unsupported WINED3DTADDRESS_U value %d\n", state);
    } else {
        if(textureDimensions==GL_TEXTURE_CUBE_MAP_ARB) {
            /* Cubemaps are always set to clamp, regardless of the sampler state. */
            wrapParm = GL_CLAMP_TO_EDGE;
        } else if(cond_np2) {
            if(state == WINED3DTADDRESS_WRAP) {
                wrapParm = GL_CLAMP_TO_EDGE;
            } else {
                wrapParm = stateLookup[WINELOOKUP_WARPPARAM][state - minLookup[WINELOOKUP_WARPPARAM]];
            }
        } else {
            wrapParm = stateLookup[WINELOOKUP_WARPPARAM][state - minLookup[WINELOOKUP_WARPPARAM]];
        }
        TRACE("Setting WRAP_S to %d for %x\n", wrapParm, textureDimensions);
        glTexParameteri(textureDimensions, type, wrapParm);
        checkGLcall("glTexParameteri(..., type, wrapParm)");
    }
}

/* GL locking is done by the caller (state handler) */
void basetexture_apply_state_changes(IWineD3DBaseTexture *iface,
        const DWORD textureStates[WINED3D_HIGHEST_TEXTURE_STATE + 1],
        const DWORD samplerStates[WINED3D_HIGHEST_SAMPLER_STATE + 1])
{
    IWineD3DBaseTextureImpl *This = (IWineD3DBaseTextureImpl *)iface;
    DWORD state, *states;
    GLint textureDimensions = IWineD3DBaseTexture_GetTextureDimensions(iface);
    BOOL cond_np2 = IWineD3DBaseTexture_IsCondNP2(iface);
    DWORD aniso;

    TRACE("iface %p, textureStates %p, samplerStates %p\n", iface, textureStates, samplerStates);

    if(This->baseTexture.is_srgb) {
        states = This->baseTexture.srgbstates;
    } else {
        states = This->baseTexture.states;
    }

    /* This function relies on the correct texture being bound and loaded. */

    if(samplerStates[WINED3DSAMP_ADDRESSU]      != states[WINED3DTEXSTA_ADDRESSU]) {
        state = samplerStates[WINED3DSAMP_ADDRESSU];
        apply_wrap(textureDimensions, state, GL_TEXTURE_WRAP_S, cond_np2);
        states[WINED3DTEXSTA_ADDRESSU] = state;
    }

    if(samplerStates[WINED3DSAMP_ADDRESSV]      != states[WINED3DTEXSTA_ADDRESSV]) {
        state = samplerStates[WINED3DSAMP_ADDRESSV];
        apply_wrap(textureDimensions, state, GL_TEXTURE_WRAP_T, cond_np2);
        states[WINED3DTEXSTA_ADDRESSV] = state;
    }

    if(samplerStates[WINED3DSAMP_ADDRESSW]      != states[WINED3DTEXSTA_ADDRESSW]) {
        state = samplerStates[WINED3DSAMP_ADDRESSW];
        apply_wrap(textureDimensions, state, GL_TEXTURE_WRAP_R, cond_np2);
        states[WINED3DTEXSTA_ADDRESSW] = state;
    }

    if(samplerStates[WINED3DSAMP_BORDERCOLOR]   != states[WINED3DTEXSTA_BORDERCOLOR]) {
        float col[4];

        state = samplerStates[WINED3DSAMP_BORDERCOLOR];
        D3DCOLORTOGLFLOAT4(state, col);
        TRACE("Setting border color for %u to %x\n", textureDimensions, state);
        glTexParameterfv(textureDimensions, GL_TEXTURE_BORDER_COLOR, &col[0]);
        checkGLcall("glTexParameteri(..., GL_TEXTURE_BORDER_COLOR, ...)");
        states[WINED3DTEXSTA_BORDERCOLOR] = state;
    }

    if(samplerStates[WINED3DSAMP_MAGFILTER]     != states[WINED3DTEXSTA_MAGFILTER]) {
        GLint glValue;
        state = samplerStates[WINED3DSAMP_MAGFILTER];
        if (state > WINED3DTEXF_ANISOTROPIC) {
            FIXME("Unrecognized or unsupported MAGFILTER* value %d\n", state);
        }

        glValue = wined3d_gl_mag_filter(This->baseTexture.magLookup,
                min(max(state, WINED3DTEXF_POINT), WINED3DTEXF_LINEAR));
        TRACE("ValueMAG=%d setting MAGFILTER to %x\n", state, glValue);
        glTexParameteri(textureDimensions, GL_TEXTURE_MAG_FILTER, glValue);

        states[WINED3DTEXSTA_MAGFILTER] = state;
    }

    if((samplerStates[WINED3DSAMP_MINFILTER]     != states[WINED3DTEXSTA_MINFILTER] ||
        samplerStates[WINED3DSAMP_MIPFILTER]     != states[WINED3DTEXSTA_MIPFILTER] ||
        samplerStates[WINED3DSAMP_MAXMIPLEVEL]   != states[WINED3DTEXSTA_MAXMIPLEVEL])) {
        GLint glValue;

        states[WINED3DTEXSTA_MIPFILTER] = samplerStates[WINED3DSAMP_MIPFILTER];
        states[WINED3DTEXSTA_MINFILTER] = samplerStates[WINED3DSAMP_MINFILTER];
        states[WINED3DTEXSTA_MAXMIPLEVEL] = samplerStates[WINED3DSAMP_MAXMIPLEVEL];

        if (states[WINED3DTEXSTA_MINFILTER] > WINED3DTEXF_ANISOTROPIC
                || states[WINED3DTEXSTA_MIPFILTER] > WINED3DTEXF_ANISOTROPIC)
        {

            FIXME("Unrecognized or unsupported D3DSAMP_MINFILTER value %d D3DSAMP_MIPFILTER value %d\n",
                  states[WINED3DTEXSTA_MINFILTER],
                  states[WINED3DTEXSTA_MIPFILTER]);
        }
        glValue = wined3d_gl_min_mip_filter(This->baseTexture.minMipLookup,
                min(max(samplerStates[WINED3DSAMP_MINFILTER], WINED3DTEXF_POINT), WINED3DTEXF_LINEAR),
                min(max(samplerStates[WINED3DSAMP_MIPFILTER], WINED3DTEXF_NONE), WINED3DTEXF_LINEAR));

        TRACE("ValueMIN=%d, ValueMIP=%d, setting MINFILTER to %x\n",
              samplerStates[WINED3DSAMP_MINFILTER],
              samplerStates[WINED3DSAMP_MIPFILTER], glValue);
        glTexParameteri(textureDimensions, GL_TEXTURE_MIN_FILTER, glValue);
        checkGLcall("glTexParameter GL_TEXTURE_MIN_FILTER, ...");

        if(!cond_np2) {
            if(states[WINED3DTEXSTA_MIPFILTER] == WINED3DTEXF_NONE) {
                glValue = This->baseTexture.LOD;
            } else if(states[WINED3DTEXSTA_MAXMIPLEVEL] >= This->baseTexture.levels) {
                glValue = This->baseTexture.levels - 1;
            } else if(states[WINED3DTEXSTA_MAXMIPLEVEL] < This->baseTexture.LOD) {
                /* baseTexture.LOD is already clamped in the setter */
                glValue = This->baseTexture.LOD;
            } else {
                glValue = states[WINED3DTEXSTA_MAXMIPLEVEL];
            }
            /* Note that D3DSAMP_MAXMIPLEVEL specifies the biggest mipmap(default 0), while
             * GL_TEXTURE_MAX_LEVEL specifies the smallest mimap used(default 1000).
             * So D3DSAMP_MAXMIPLEVEL is the same as GL_TEXTURE_BASE_LEVEL.
             */
            glTexParameteri(textureDimensions, GL_TEXTURE_BASE_LEVEL, glValue);
        }
    }

    if ((states[WINED3DSAMP_MAGFILTER] != WINED3DTEXF_ANISOTROPIC
            && states[WINED3DSAMP_MINFILTER] != WINED3DTEXF_ANISOTROPIC
            && states[WINED3DSAMP_MIPFILTER] != WINED3DTEXF_ANISOTROPIC)
            || cond_np2)
    {
        aniso = 1;
    }
    else
    {
        aniso = samplerStates[WINED3DSAMP_MAXANISOTROPY];
    }

    if (states[WINED3DTEXSTA_MAXANISOTROPY] != aniso)
    {
        if (GL_SUPPORT(EXT_TEXTURE_FILTER_ANISOTROPIC))
        {
            glTexParameteri(textureDimensions, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso);
            checkGLcall("glTexParameteri(GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso)");
        }
        else
        {
            WARN("Anisotropic filtering not supported.\n");
        }
        states[WINED3DTEXSTA_MAXANISOTROPY] = aniso;
    }
}
