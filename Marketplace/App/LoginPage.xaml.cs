using Xamarin.Forms;
using Xamarin.Forms.Xaml;

using Marktplaats.Data.Authentication;
using Marktplaats.Data;

namespace Marktplaats.Views
{
    [XamlCompilation(XamlCompilationOptions.Compile)]
    public partial class LoginPage : ContentPage
    {
        // This bool will prevent the login screen from reopening before the process has finished
        private bool triedToAuthenticate = false;

        public LoginPage()
        {
            InitializeComponent();
        }

        protected override async void OnAppearing()
        {
            base.OnAppearing();

            if (!triedToAuthenticate)
            {
                triedToAuthenticate = true;

                var auth = DependencyService.Get<IAuthenticator>();
                if (auth != null)
                {
                    var authority = RestService.AppSettings["Adfs.authority"];
                    var clientId = RestService.AppSettings["Adfs.clientId"];
                    var returnUri = RestService.AppSettings["Adfs.returnUri"];
                    var resource = RestService.AppSettings["Adfs.resource"];

                    // Authenticate the user through ADFS
                    var authResult = await auth.Authenticate(authority, resource, clientId, returnUri);
                    if (authResult != null)
                    {
                        RestService.AuthHeader = authResult.CreateAuthorizationHeader();

                        // Make sure the user exists in the API
                        var userService = new UserService();
                        var user = await userService.GetUser();
                        if (user != null)
                        {
                            RestService.AuthUser = user;

                            await Navigation.PushAsync(new MarktplaatsPage());
                            Navigation.RemovePage(this);
                            return;
                        }
                    }
                }

                activityIndicator.IsVisible = false;
                errorLabel.Text = RestService.AppSettings["ErrorMessage.Auth.Failed"];
                errorLabel.TextColor = Color.Red;
            }
        }
    }
}
