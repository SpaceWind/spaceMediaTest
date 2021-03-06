import QtQuick 2.2
import QtQuick.Controls 1.1
import QtWebView 1.1
//import QtWebKit 3.0
import QtQuick.Layouts 1.1
import QtQuick.Controls.Styles 1.2

Item{
    width: parent.width
    height: parent.height
    property string prevUrl:""
    function load(url){
        if (prevUrl === url){
            console.log("WebBrowser::load -> realoding page")
            webView.reload()
        }
        else
        {
            prevUrl = url
            webView.url = url
        }
    }

    function setShowTime(msecs){
        if (msecs < 1000)
            msecs = 10000
        turnOffTimer.interval = msecs
        console.log("browser show time = " + msecs)
    }
    function startShow(){
        turnOffTimer.stop()
        turnOffTimer.start()
    }
    function stopBrowser(){
        turnOffTimer.stop()
    }

    property bool isActive: false

    signal loadFinished()
    signal endShow()

    Timer {
        id: turnOffTimer
        repeat: false
        onTriggered: {
            endShow()
            isActive = false
        }
    }

    WebView {
        id: webView
        visible: true
        anchors.fill: parent
        url: ""
        //enabled: false
    }
}
