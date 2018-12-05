# RF Example Application

![Example App](./RF_example.png)

- rf_example.cpp is an example application running very basic features of the RF library to bootstrap new projects. It should draw some UI as well as a colored 2D quad. The UI won't display text if no font is placed in the executable directory and linked with the UI theme config file.
- ui_config.json is an example theme config file for the RF UI. RF handles TTF fonts, as well as Unicode character printing (like [FontAwesome](https://fontawesome.com/)), these should be put in the application path and linked with the ui theme config file.