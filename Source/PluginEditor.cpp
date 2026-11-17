#include "PluginEditor.h"
#include "BinaryData.h"

YoloEditor::YoloEditor (YoloProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      browser (juce::WebBrowserComponent::Options{}
          .withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
          .withWinWebView2Options (
              juce::WebBrowserComponent::Options::WinWebView2{}
                  .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                           .getChildFile ("YoloVST3WebView2")))
          .withResourceProvider ([] (const juce::String& resourcePath)
          {
              if (resourcePath != "/" && resourcePath != "/index.html")
                  return std::optional<juce::WebBrowserComponent::Resource>{};

              juce::WebBrowserComponent::Resource resource;
              const auto* begin = reinterpret_cast<const std::byte*> (BinaryData::index_html);
              resource.data.assign (begin, begin + BinaryData::index_htmlSize);
              resource.mimeType = "text/html";
              return std::optional<juce::WebBrowserComponent::Resource> (std::move (resource));
          }))
{
    setSize (1100, 720);
    addAndMakeVisible (browser);
    browser.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
}

void YoloEditor::resized()
{
    browser.setBounds (getLocalBounds());
}
