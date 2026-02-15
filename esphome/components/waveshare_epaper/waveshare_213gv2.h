// Add this class declaration to waveshare_epaper.h
// Place it near other 2.13" display classes (around line 1044)

class WaveshareEPaper2P13InGV2 : public WaveshareEPaper7C {
 public:
  void setup() override;
  
  void initialize() override;

  void display() override;

  void dump_config() override;

  void deep_sleep() override;
  
  void fill(Color color) override;

 protected:
  int get_width_internal() override;

  int get_height_internal() override;
  
  uint32_t get_buffer_length_() override;
  
  uint32_t idle_timeout_() override;
  
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  
  uint8_t color_to_4color(Color color);
};
