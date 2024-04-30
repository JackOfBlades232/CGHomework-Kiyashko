#include "shadowmap_render.h"

#include "../../render/render_gui.h"


void SimpleShadowmapRender::DoImGUI()
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  {
    static bool inited = false;
    if (!inited) {
      m_light.cam.pos = LiteMath::float3(-1.8f, 1.4f, 3.5f);
      inited = true;
    }

    ImGui::Begin("Simple render settings");

    ImGui::ColorEdit3("Meshes base color", m_uniforms.baseColor.M, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);
    ImGui::SameLine();
    ImGui::ColorEdit3("Ambient light color", ambientLightColor.M, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);

    ImGui::SliderFloat("Light source intensity coeff", &lightIntensity, 0.f, 50.f);
    ImGui::SliderFloat("Ambient light intensity coeff", &ambientIntensity, 0.f, 10.f);

    ImGui::NewLine();

    ImGui::SliderFloat3("Light source position", m_light.cam.pos.M, -10.f, 10.f);
    ImGui::SliderFloat2("Min/max terrain height", terrainMinMaxHeight.M, -10.f, 10.f);
    if (terrainMinMaxHeight.x > terrainMinMaxHeight.y)
      terrainMinMaxHeight.x = terrainMinMaxHeight.y;
    ImGui::Checkbox("Enable RSM", &useRsm);
    ImGui::Checkbox("Enable SSAO", &useSsao);
    ImGui::Checkbox("Enable fog", &volfogEnabled);

    ShadowmapChoiceGUI();
    AAChoiceGui();
    TonemappingChoiceGui();

    ImGui::NewLine();

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");
    ImGui::End();
  }

  // @TODO(PKiyashko): all the reloads should be more consolidated and at the right time. Look into how shaders are recompiled.
  if (settingsAreDirty)
  {
    settingsAreDirty = false;

    RebuildCurrentDeferredPipelines();
    ReallocateVolfogResources();
    resetReprojection = true;
  }

  // Rendering
  ImGui::Render();
}

void SimpleShadowmapRender::ShadowmapChoiceGUI()
{
  const char *items[eShTechMax] = { "None", "Simple", "VSM", "PCF" };
  const char *currentItem       = items[currentShadowmapTechnique];

  ImGuiStyle &style = ImGui::GetStyle();
  float w           = ImGui::CalcItemWidth();
  float buttonSize  = ImGui::GetFrameHeight();
  float spacing     = style.ItemInnerSpacing.x;
  ImGui::PushItemWidth(w - 2.0f * spacing - 2.0f * buttonSize);
  if (ImGui::BeginCombo("##shadow technique", currentItem, ImGuiComboFlags_NoArrowButton))
  {
    for (int i = 0; i < IM_ARRAYSIZE(items); i++)
    {
      bool selected = (currentItem == items[i]);
      if (ImGui::Selectable(items[i], selected))
      {
        currentItem               = items[i];
        if (currentShadowmapTechnique != i)
          settingsAreDirty = true;
        currentShadowmapTechnique = (ShadowmapTechnique)i;
      }
      if (selected)
      {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  ImGui::PopItemWidth();
  ImGui::SameLine(0, style.ItemInnerSpacing.x);
  ImGui::Text("Shadow technique");
}

void SimpleShadowmapRender::AAChoiceGui()
{
  const char *items[]     = { "None", "SSAAx4", "TAA (attempt)" };
  const char *currentItem = items[currentAATechnique];

  ImGuiStyle &style = ImGui::GetStyle();
  float w           = ImGui::CalcItemWidth();
  float buttonSize  = ImGui::GetFrameHeight();
  float spacing     = style.ItemInnerSpacing.x;

  ImGui::PushItemWidth(w - 2.0f * spacing - 2.0f * buttonSize);
  if (ImGui::BeginCombo("##AA technique", currentItem, ImGuiComboFlags_NoArrowButton))
  {
    for (int i = 0; i < IM_ARRAYSIZE(items); i++)
    {
      bool selected = (currentItem == items[i]);
      if (ImGui::Selectable(items[i], selected))
      {
        currentItem        = items[i];
        if (currentAATechnique != i)
          settingsAreDirty = true;
        currentAATechnique = (AATechnique)i;
      }
      if (selected)
      {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  ImGui::PopItemWidth();

  ImGui::SameLine(0, style.ItemInnerSpacing.x);
  ImGui::Text("Anti-aliasing technique");

  if (currentAATechnique == eTaa)
  {
    ImGui::SliderFloat("TAA reprojection coeff", &currentReprojectionCoeff, 0.0f, 1.0f);
  }
}

void SimpleShadowmapRender::TonemappingChoiceGui()
{
  const char* items[] = { "None", "Reinhard", "Exposure" };
  const char* currentItem = items[currentTonemappingTechnique];

  ImGuiStyle& style = ImGui::GetStyle();
  float w = ImGui::CalcItemWidth();
  float buttonSize = ImGui::GetFrameHeight();
  float spacing = style.ItemInnerSpacing.x;

  ImGui::PushItemWidth(w - 2.0f * spacing - 2.0f * buttonSize);
  if (ImGui::BeginCombo("##Tonemapping technique", currentItem, ImGuiComboFlags_NoArrowButton))
  {
    for (int i = 0; i < IM_ARRAYSIZE(items); i++)
    {
      bool selected = (currentItem == items[i]);
      if (ImGui::Selectable(items[i], selected))
      {
        currentItem = items[i];
        if (currentTonemappingTechnique != i)
          settingsAreDirty = true;
        currentTonemappingTechnique = (TonemappingTechnique)i;
      }
      if (selected)
      {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  ImGui::PopItemWidth();

  ImGui::SameLine(0, style.ItemInnerSpacing.x);
  ImGui::Text("Tonemapping technique");

  if (currentTonemappingTechnique == eExposure)
  {
    ImGui::SliderFloat("Exposure coeff", &exposureCoeff, 0.0f, 15.0f);
  }
}
