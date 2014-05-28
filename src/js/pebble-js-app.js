var mConfig = {};

Pebble.addEventListener("ready", function(e) {
  loadLocalData();
  returnConfigToPebble();
});

Pebble.addEventListener("showConfiguration", function(e) {
	Pebble.openURL(mConfig.configureUrl);
});

Pebble.addEventListener("webviewclosed",
  function(e) {
    if (e.response) {
      var config = JSON.parse(e.response);
      saveLocalData(config);
      returnConfigToPebble();
    }
  }
);

function saveLocalData(config) {
  
  localStorage.setItem("invert", parseInt(config.invert)); 
  loadLocalData();
}
function loadLocalData() {
	mConfig.invert = parseInt(localStorage.getItem("invert"));
	mConfig.configureUrl = "http://www.pebbletr.com/settings/index.html";

	if(isNaN(mConfig.invert)) {
		mConfig.invert = 0;
	}
}
function returnConfigToPebble() {
  Pebble.sendAppMessage({
    "invert":parseInt(mConfig.invert), 
  });    
}
