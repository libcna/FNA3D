/* SPDX-License-Identifier: Zlib */
#include "../../MojoShader/tests/cna/cna_sampler_inference_effect.h"
/* Public FNA3D_CreateEffect path. Cube: byte RGB (32,96,160) selects +Z;
 * u=(32/160+1)/2=.6, v=(1-96/160)/2=.2 => texel (2,0).
 * _bx2=(-191,-63,65)/255 selects -X; u=(65/191+1)/2, v=(63/191+1)/2
 * => (2,2). Volume: floor(4*RGB/255)=(0,1,2); _bx2 clamps to (0,0,1).
 * Grid encodes x,y,z/face independently; every target pixel must match. */
static void SamplerInferencePixels(void)
{
    static const uint32_t kinds[]={MOJOSHADER_SYMTYPE_TEXTURE2D,MOJOSHADER_SYMTYPE_TEXTURECUBE,
                                  MOJOSHADER_SYMTYPE_TEXTURE3D};
    static const uint8_t expected[3][2][4]={{{40,80,14,255},{40,40,7,255}},
        {{120,40,164,255},{120,120,76,255}},{{40,80,94,255},{40,40,47,255}}};
    uint8_t sourceBytes[]={32,96,160,224}, grid[4*4*4*4], pixels[TW*TH*4];
    FNA3D_Texture *source=FNA3D_CreateTexture2D(device,FNA3D_SURFACEFORMAT_COLOR,1,1,1,0);
    FNA3D_Texture *lookup[3];
    FNA3D_SamplerState sampler;
    FNA3D_Vec4 clear={.2f,.4f,.6f,.8f};
    int kind, signedSource, reverse, large, x,y,z, cases=0;
    lookup[0]=FNA3D_CreateTexture2D(device,FNA3D_SURFACEFORMAT_COLOR,4,4,1,0);
    lookup[1]=FNA3D_CreateTextureCube(device,FNA3D_SURFACEFORMAT_COLOR,4,1,0);
    lookup[2]=FNA3D_CreateTexture3D(device,FNA3D_SURFACEFORMAT_COLOR,4,4,4,1);
    Check(source && lookup[0] && lookup[1] && lookup[2],"inference textures exist");
    if(!source || !lookup[0] || !lookup[1] || !lookup[2]) goto done;
    FNA3D_SetTextureData2D(device,source,0,0,1,1,0,sourceBytes,4);
    for(z=0;z<4;z++) for(y=0;y<4;y++) for(x=0;x<4;x++)
    {
        uint8_t *p=grid+4*(16*z+4*y+x);
        p[0]=40*(x+1); p[1]=40*(y+1); p[2]=7*(4*x+y+1)+40*z; p[3]=255;
    }
    FNA3D_SetTextureData2D(device,lookup[0],0,0,4,4,0,grid,64);
    FNA3D_SetTextureData3D(device,lookup[2],0,0,0,4,4,4,0,grid,sizeof(grid));
    for(z=0;z<6;z++)
    {
        for(y=0;y<4;y++) for(x=0;x<4;x++)
        {
            uint8_t *p=grid+4*(4*y+x);
            p[0]=40*(x+1);p[1]=40*(y+1);p[2]=30*z+4*x+y+36;p[3]=255;
        }
        FNA3D_SetTextureDataCube(device,lookup[1],0,0,4,4,(FNA3D_CubeMapFace)z,0,grid,64);
    }
    SDL_zero(sampler); sampler.filter=FNA3D_TEXTUREFILTER_POINT;
    sampler.addressU=sampler.addressV=sampler.addressW=FNA3D_TEXTUREADDRESSMODE_CLAMP;
    sampler.maxAnisotropy=1;
    for(kind=0;kind<3;kind++) for(signedSource=0;signedSource<2;signedSource++)
    for(reverse=0;reverse<2;reverse++) for(large=0;large<2;large++)
    {
        CnaSamplerEffect code;
        MOJOSHADER_effect *data=NULL;
        TestEffect_Vertex vertices[3];
        const float coords[]={.5f,.5f,0,1};
        unsigned int word;
        int same=1, i;
        cna_sampler_effect(&code,kinds[kind],reverse,large,0);
        if(signedSource) for(word=0;word+2<code.length/4;word++)
            if(code.words[word]==82 && code.words[word+1]==0xB00F0001)
                code.words[word+2]=0xB4E40000;
        FNA3D_CreateEffect(device,(uint8_t *)code.words,code.length,&effect,&data);
        Check(effect && data && !data->error_count,"inferred public effect compiles");
        if(!effect || !data || data->error_count)
        {
            if(data && data->error_count) fprintf(stderr,"%s\n",data->errors[0].error);
            if(effect) FNA3D_AddDisposeEffect(device,effect);
            continue;
        }
        TestEffect_FullscreenTriangle(vertices,coords);
        vb=FNA3D_GenVertexBuffer(device,0,FNA3D_BUFFERUSAGE_WRITEONLY,sizeof(vertices));
        Check(vb!=NULL,"inference vertex buffer exists");
        if(!vb) { FNA3D_AddDisposeEffect(device,effect);continue; }
        FNA3D_SetVertexBufferData(device,vb,0,vertices,3,sizeof(vertices[0]),sizeof(vertices[0]),FNA3D_SETDATAOPTIONS_NONE);
        FNA3D_Clear(device,FNA3D_CLEAROPTIONS_TARGET,&clear,1,0);
        FNA3D_VerifySampler(device,1,lookup[kind],&sampler);
        Draw(source,0,0,TW);
        FNA3D_ReadBackbuffer(device,0,0,TW,TH,pixels,sizeof(pixels));
        for(i=0;i<TW*TH;i++) if(SDL_memcmp(pixels+4*i,expected[kind][signedSource],4)) {same=0;break;}
        if(!same) fprintf(stderr,"inference kind=%d signed=%d reverse=%d large=%d pixel=%d got=(%u,%u,%u,%u)\n",
            kind,signedSource,reverse,large,i,pixels[4*i],pixels[4*i+1],pixels[4*i+2],pixels[4*i+3]);
        Check(same,"every inferred sampler pixel matches hand-derived axes/slices");
        EndProgram(); cases++;
    }
    Check(cases==24,"all 24 public inference draws executed");
done:
    if(source)FNA3D_AddDisposeTexture(device,source);
    for(kind=0;kind<3;kind++)if(lookup[kind])FNA3D_AddDisposeTexture(device,lookup[kind]);
}
