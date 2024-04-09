#include "shadowmap_render.h"

#include "../../render/render_gui.h"


void SimpleShadowmapRender::DoImGUI()
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  {
//    ImGui::ShowDemoWindow();
    ImGui::Begin("Simple render settings");

    ImGui::ColorEdit3("Meshes base color", m_uniforms.baseColor.M, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoInputs);
    ImGui::SliderFloat3("Light source position", m_light.cam.pos.M, -10.f, 10.f);

    ShadowmapChoiceGUI();
    AAChoiceGui();

    ImGui::NewLine();

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),"Press 'B' to recompile and reload shaders");
    ImGui::End();
  }

  // @TODO(PKiyashko): all the reloads should be more consolidated and at the right time. Look into how shaders are recompiled.
  if (settingsAreDirty)
  {
    settingsAreDirty = false;
    RebuildCurrentForwardPipeline();
    resetReprojection = true;
  }

  // Rendering
  ImGui::Render();
}

void SimpleShadowmapRender::ShadowmapChoiceGUI()
{
  const char *items[eShTechMax]  = { "Simple", "VSM", "PCF" };
  static const char *currentItem = items[currentShadowmapTechnique];

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
  const char *items[]            = { "None", "SSAAx4", "MSAAx4", "TAA (attempt)" };
  static const char *currentItem = items[currentAATechnique];

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
