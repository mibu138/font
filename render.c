#include "render.h"
#include "tanto/m_math.h"
#include "tanto/r_geo.h"
#include "tanto/v_image.h"
#include "tanto/v_memory.h"
#include <memory.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <tanto/r_render.h>
#include <tanto/t_text.h>
#include <tanto/v_video.h>
#include <tanto/t_def.h>
#include <tanto/t_utils.h>
#include <tanto/r_pipeline.h>
#include <tanto/r_raytrace.h>
#include <tanto/r_renderpass.h>
#include <tanto/v_command.h>
#include <vulkan/vulkan_core.h>
#include <stdlib.h>

#define SPVDIR "./shaders/spv"
#define MAX_QUADS 8

static Tanto_V_Image attachmentText;
static Tanto_V_Image attachmentDepth;

static VkRenderPass  renderpass;
static VkFramebuffer framebuffers[TANTO_FRAME_COUNT];

static VkPipeline    mainPipeline;
static VkPipeline    cardPipeline;

static Tanto_V_BufferRegion uniformBufferRegion;

static struct {
    uint8_t quadCount;
    Tanto_R_Primitive quad[MAX_QUADS];
    Tanto_R_VertexDescription vertDescription;
} cards;

typedef enum {
    R_PIPE_LAYOUT_MAIN,
} R_PipelineLayoutId;

typedef enum {
    R_DESC_SET_MAIN,
} R_DescriptorSetId;

// TODO: we should implement a way to specify the offscreen renderpass format at initialization
static void initAttachments(void)
{
    attachmentDepth = tanto_v_CreateImage(
        TANTO_WINDOW_WIDTH, TANTO_WINDOW_HEIGHT,
        tanto_r_GetDepthFormat(),
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_SAMPLE_COUNT_1_BIT);

    attachmentText = tanto_CreateTextImage(TANTO_WINDOW_WIDTH, TANTO_WINDOW_WIDTH, 10, 500, 240, "Butt.");
}

static void initCards(void)
{
    cards.quadCount = 10;
    for (int i = 0; i < cards.quadCount; i++) 
    {
        float x = ((float)random() / (float)RAND_MAX);
        float y = ((float)random() / (float)RAND_MAX);
        x = 2 * x - 1;
        y = 2 * y - 1;
        cards.quad[i] = tanto_r_CreateQuadNDC(x, y, 0.8, 0.8, &cards.vertDescription);
    }
}

static void initRenderPass(void)
{
    const VkAttachmentDescription attachmentColor = {
        .flags = 0,
        .format = tanto_r_GetSwapFormat(),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    const VkAttachmentDescription attachmentDepth = {
        .flags = 0,
        .format = tanto_r_GetDepthFormat(),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    const VkAttachmentDescription attachments[] = {
        attachmentColor, attachmentDepth
    };

    VkAttachmentReference colorReference = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference depthReference = {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    const VkSubpassDescription subpass = {
        .flags                   = 0,
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount    = 0,
        .pInputAttachments       = NULL,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &colorReference,
        .pResolveAttachments     = NULL,
        .pDepthStencilAttachment = &depthReference,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments    = NULL,
    };

    Tanto_R_RenderPassInfo rpi = {
        .attachmentCount = 2,
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
    };

    tanto_r_CreateRenderPass(&rpi, &renderpass);
}

static void initFramebuffers(void)
{
    for (int i = 0; i < TANTO_FRAME_COUNT; i++) 
    {
        Tanto_R_Frame* frame = tanto_r_GetFrame(i);
        const VkImageView attachments[] = {
            frame->swapImage.view, attachmentDepth.view
        };

        const VkFramebufferCreateInfo fbi = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
            .renderPass = renderpass,
            .attachmentCount = 2,
            .pAttachments = attachments,
            .width = TANTO_WINDOW_WIDTH,
            .height = TANTO_WINDOW_HEIGHT,
            .layers = 1,
        };

        V_ASSERT( vkCreateFramebuffer(device, &fbi, NULL, &framebuffers[i]) );
    }
}

static void initDescriptorSetsAndPipelineLayouts(void)
{
    const Tanto_R_DescriptorSet descriptorSets[] = {{
        .id = R_DESC_SET_MAIN,
        .bindingCount = 2,
        .bindings = {{
            .descriptorCount = 1,
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
        },{
            .descriptorCount = 1,
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
        }}
    }};

    const Tanto_R_PipelineLayout pipelayouts[] = {{
        .id = R_PIPE_LAYOUT_MAIN, 
        .descriptorSetCount = 1, 
        .descriptorSetIds = {R_DESC_SET_MAIN},
        .pushConstantCount = 0,
        .pushConstantsRanges = {}
    }};

    tanto_r_InitDescriptorSets(descriptorSets, TANTO_ARRAY_SIZE(descriptorSets));
    tanto_r_InitPipelineLayouts(pipelayouts, TANTO_ARRAY_SIZE(pipelayouts));
}

static void initPipelines(void)
{
    const Tanto_R_PipelineInfo pipeInfo = {
        .type     = TANTO_R_PIPELINE_POSTPROC_TYPE,
        .layoutId = R_PIPE_LAYOUT_MAIN,
        .payload.rasterInfo = {
            .renderPass = renderpass, 
            .sampleCount = VK_SAMPLE_COUNT_1_BIT,
            .fragShader = SPVDIR"/text-frag.spv"
        }
    };

    const Tanto_R_PipelineInfo cardPipeInfo = {
        .type     = TANTO_R_PIPELINE_RASTER_TYPE,
        .layoutId = R_PIPE_LAYOUT_MAIN,
        .payload.rasterInfo = {
            .renderPass = renderpass, 
            .sampleCount = VK_SAMPLE_COUNT_1_BIT,
            .vertexDescription = cards.vertDescription,
            .vertShader = SPVDIR"/card-vert.spv",
            .fragShader = SPVDIR"/card-frag.spv"
        }
    };

    tanto_r_CreatePipeline(&pipeInfo, &mainPipeline);
    tanto_r_CreatePipeline(&cardPipeInfo, &cardPipeline);
}

// descriptors that do only need to have update called once and can be updated on initialization
static void updateStaticDescriptors(void)
{
    uniformBufferRegion = tanto_v_RequestBufferRegion(sizeof(UniformBuffer), 
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, TANTO_V_MEMORY_HOST_GRAPHICS_TYPE);
    memset(uniformBufferRegion.hostData, 0, sizeof(Parms));
    UniformBuffer* uboData = (UniformBuffer*)(uniformBufferRegion.hostData);

    Mat4 view = m_Ident_Mat4();
    view = m_Translate_Mat4((Vec3){0, 0, -1}, &view);

    uboData->matModel = m_Ident_Mat4();
    uboData->matView  = view;
    uboData->matProj  = m_BuildPerspective(0.001, 100);

    VkDescriptorBufferInfo uboInfo = {
        .buffer = uniformBufferRegion.buffer,
        .offset = uniformBufferRegion.offset,
        .range  = uniformBufferRegion.size
    };

    VkWriteDescriptorSet writes[] = {{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstArrayElement = 0,
        .dstSet = descriptorSets[R_DESC_SET_MAIN],
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &uboInfo
    }};

    vkUpdateDescriptorSets(device, TANTO_ARRAY_SIZE(writes), writes, 0, NULL);
}

static void updateDynamicDescriptors(void)
{
    VkDescriptorImageInfo imageInfo = {
        .imageView = attachmentText.view,
        .imageLayout = attachmentText.layout,
        .sampler = attachmentText.sampler
    };

    VkWriteDescriptorSet writes[] = {{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstArrayElement = 0,
        .dstSet = descriptorSets[R_DESC_SET_MAIN],
        .dstBinding = 1,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &imageInfo
    }};

    vkUpdateDescriptorSets(device, TANTO_ARRAY_SIZE(writes), writes, 0, NULL);
}

static void mainRender(const VkCommandBuffer* cmdBuf, const VkRenderPassBeginInfo* rpassInfo)
{
    vkCmdBindPipeline(*cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, mainPipeline);

    vkCmdBindDescriptorSets(
        *cmdBuf, 
        VK_PIPELINE_BIND_POINT_GRAPHICS, 
        pipelineLayouts[R_PIPE_LAYOUT_MAIN], 
        0, 1, &descriptorSets[R_DESC_SET_MAIN],
        0, NULL);

    vkCmdBeginRenderPass(*cmdBuf, rpassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdDraw(*cmdBuf, 3, 1, 0, 0);

    vkCmdBindPipeline(*cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, cardPipeline);

    for (int i = 0; i < 8; i++) 
    {
        Tanto_R_Primitive prim = cards.quad[i];

        const VkBuffer vertBuffers[2] = {
            prim.vertexRegion.buffer,
            prim.vertexRegion.buffer
        };

        const VkDeviceSize attrOffsets[2] = {
            prim.attrOffsets[0] + prim.vertexRegion.offset,
            prim.attrOffsets[1] + prim.vertexRegion.offset,
        };

        vkCmdBindVertexBuffers(*cmdBuf, 0, 2, vertBuffers, attrOffsets);

        vkCmdBindIndexBuffer(*cmdBuf, prim.indexRegion.buffer, 
                prim.indexRegion.offset, TANTO_VERT_INDEX_TYPE);

        vkCmdDrawIndexed(*cmdBuf, prim.indexCount, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(*cmdBuf);
}

void r_InitRenderer()
{
    initAttachments();
    initRenderPass();
    initFramebuffers();
    initDescriptorSetsAndPipelineLayouts();
    initCards();
    initPipelines();
    updateStaticDescriptors();
    updateDynamicDescriptors();
}

void r_UpdateRenderCommands(const int8_t frameIndex)
{
    Tanto_R_Frame* frame = tanto_r_GetFrame(frameIndex);
    vkResetCommandPool(device, frame->commandPool, 0);
    VkCommandBufferBeginInfo cbbi = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    V_ASSERT( vkBeginCommandBuffer(frame->commandBuffer, &cbbi) );

    VkClearValue clearValueColor = {0.002f, 0.023f, 0.009f, 1.0f};
    VkClearValue clearValueDepth = {1.0, 0};

    VkClearValue clears[] = {clearValueColor, clearValueDepth};

    const VkRenderPassBeginInfo rpassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .clearValueCount = 2,
        .pClearValues = clears,
        .renderArea = {{0, 0}, {TANTO_WINDOW_WIDTH, TANTO_WINDOW_HEIGHT}},
        .renderPass =  renderpass,
        .framebuffer = framebuffers[frameIndex],
    };

    mainRender(&frame->commandBuffer, &rpassInfo);

    V_ASSERT( vkEndCommandBuffer(frame->commandBuffer) );
}

void r_RecreateSwapchain(void)
{
    vkDeviceWaitIdle(device);
    r_CleanUp();
    printf("CLEANUP CALLED!\n");

    tanto_r_RecreateSwapchain();
    initAttachments();
    initPipelines();
    initFramebuffers();
    updateDynamicDescriptors();

    for (int i = 0; i < TANTO_FRAME_COUNT; i++) 
    {
        r_UpdateRenderCommands(i);
    }
}

void r_CleanUp(void)
{
    for (int i = 0; i < TANTO_FRAME_COUNT; i++) 
    {
        vkDestroyFramebuffer(device, framebuffers[i], NULL);
    }
    tanto_v_FreeImage(&attachmentDepth);
    tanto_v_FreeImage(&attachmentText);
    vkDestroyPipeline(device, mainPipeline, NULL);
}
