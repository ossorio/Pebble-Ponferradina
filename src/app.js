Pebble.addEventListener('showConfiguration', function() {
  var url = 'https://dl.dropboxusercontent.com/u/14197480/Configuracion-app-pebble/config.html';

  Pebble.openURL(url);
});

Pebble.addEventListener('webviewclosed', function(e) {
  // Decodifica las preferencias del usuario
  // Decode the user's preferences
  var configData = JSON.parse(decodeURIComponent(e.response));
  
  // Envia los datos via AppMessage
                        
  var dict = {
    'KEY_FREQUENCY_UPDATE_WEATHER': configData.freqUpdateWeather
};


//Envia los datos
Pebble.sendAppMessage(dict, function() {
  console.log('Config data sent successfully!');
}, function(e) {
  console.log('Error sending config data!');
});
  
});