(function ($){
	Drupal.behaviors.wysiwyg_accordion_theme_createAccordions = {
		attach:function (context) {
            $( "div.wysiwyg_accordion" ).accordion({
      collapsible: true
    });
		}
	}
}(jQuery));
